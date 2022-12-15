#include "stdafx.h"

#include "JXLWrapper.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "MaxImageDef.h"
#include <stdlib.h>
#include <vector>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

JxlDecoderPtr cached_jxl_decoder = NULL;
JxlResizableParallelRunnerPtr cached_jxl_runner = NULL;
JxlBasicInfo cached_jxl_info = {0};
uint8_t* cached_jxl_data = NULL;
size_t cached_jxl_data_size = 0;
int cached_jxl_prev_frame_timestamp = 0;
int cached_jxl_width = 0;
int cached_jxl_height = 0;

/** Decodes JPEG XL image to floating point pixels and ICC Profile. Pixel are
 * stored as floating point, as interleaved RGBA (4 floating point values per
 * pixel), line per line from top to bottom.  Pixel values have nominal range
 * 0..1 but may go beyond this range for HDR or wide gamut. The ICC profile
 * describes the color format of the pixel data.
 */

bool DecodeJpegXlOneShot(const uint8_t* jxl, size_t size, std::vector<uint8_t>* pixels, int& xsize,
    int& ysize, bool& have_animation, int& frame_count, int& frame_time, std::vector<uint8_t>* icc_profile, bool& outOfMemory) {

    if (cached_jxl_decoder.get() == NULL) {
        cached_jxl_runner = JxlResizableParallelRunnerMake(nullptr);

        // JxlDecoderRewind(JxlDecoder* cached_jxl_decoder);

        cached_jxl_decoder = JxlDecoderMake(nullptr);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderSubscribeEvents(cached_jxl_decoder.get(), JXL_DEC_BASIC_INFO |
                JXL_DEC_COLOR_ENCODING |
                JXL_DEC_FRAME |
                JXL_DEC_FULL_IMAGE)) {
            fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
            return false;
        }


        if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(cached_jxl_decoder.get(),
            JxlResizableParallelRunner,
            cached_jxl_runner.get())) {
            fprintf(stderr, "JxlDecoderSetParallelRunner failed\n");
            return false;
        }

        

        JxlDecoderSetInput(cached_jxl_decoder.get(), jxl, size);
        JxlDecoderCloseInput(cached_jxl_decoder.get());
        cached_jxl_data = (uint8_t*)jxl;
        cached_jxl_data_size = size;
    }

    JxlBasicInfo info;
    JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(cached_jxl_decoder.get());

        if (status == JXL_DEC_ERROR) {
            fprintf(stderr, "Decoder error\n");
            return false;
        }
        else if (status == JXL_DEC_NEED_MORE_INPUT) {
            fprintf(stderr, "Error, already provided all input\n");
            return false;
        }
        else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(cached_jxl_decoder.get(), &info)) {
                fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
                return false;
            }
            cached_jxl_info = info;
            if (abs((double)cached_jxl_info.xsize * cached_jxl_info.ysize) > MAX_IMAGE_PIXELS) {
                outOfMemory = true;
                return false;
            }
            JxlResizableParallelRunnerSetThreads(
                cached_jxl_runner.get(),
                JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        }
        else if (status == JXL_DEC_COLOR_ENCODING) {
            // Get the ICC color profile of the pixel data
            size_t icc_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderGetICCProfileSize(
                    cached_jxl_decoder.get(), &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
                fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
                return false;
            }
            icc_profile->resize(icc_size);
            if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                cached_jxl_decoder.get(), &format,
                JXL_COLOR_PROFILE_TARGET_DATA,
                icc_profile->data(), icc_profile->size())) {
                fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
                return false;
            }
        }
        else if (status == JXL_DEC_FRAME) {
            JxlFrameHeader header;
            JxlDecoderGetFrameHeader(cached_jxl_decoder.get(), &header);
            // TODO
            JxlAnimationHeader animation = cached_jxl_info.animation;
            frame_time = 1000 * (uint64_t)header.duration * (uint64_t)animation.tps_denominator / (double)animation.tps_numerator;
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(cached_jxl_decoder.get(), &format, &buffer_size)) {
                fprintf(stderr, "JxlDecoderImageOutBufferSize failed\n");
                return false;
            }
            if (buffer_size != cached_jxl_info.xsize * cached_jxl_info.ysize * 4) {
                fprintf(stderr, "Invalid out buffer size %" "llu" " %" "llu" "\n",
                    static_cast<uint64_t>(buffer_size),
                    static_cast<uint64_t>(cached_jxl_info.xsize * cached_jxl_info.ysize * 4));
                return false;
            }
            pixels->resize(cached_jxl_info.xsize * cached_jxl_info.ysize * 4);
            void* pixels_buffer = (void*)pixels->data();
            size_t pixels_buffer_size = pixels->size() * sizeof(uint8_t);
            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(cached_jxl_decoder.get(), &format,
                pixels_buffer,
                pixels_buffer_size)) {
                fprintf(stderr, "JxlDecoderSetImageOutBuffer failed\n");
                return false;
            }
            // return true;
        }
        else if (status == JXL_DEC_FULL_IMAGE) {
            // Nothing to do. Do not yet return. If the image is an animation, more
            // full frames may be decoded. This example only keeps the last one.
            
            xsize = cached_jxl_info.xsize;
            ysize = cached_jxl_info.ysize;
            have_animation = cached_jxl_info.have_animation;
            if (have_animation) {
                frame_count = 2; // TODO get accurate frame counts
            }
            else {
                frame_count = 1;
            }
            return true;
        }
        else if (status == JXL_DEC_SUCCESS) {
            // All decoding successfully finished.
            // It's not required to call JxlDecoderReleaseInput(cached_jxl_decoder.get()) here since
            // the decoder will be destroyed.
            // JxlDecoderReleaseInput(cached_jxl_decoder.get());
            JxlDecoderRewind(cached_jxl_decoder.get());
            JxlDecoderSubscribeEvents(cached_jxl_decoder.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
            // JxlDecoderReleaseInput(cached_jxl_decoder.get());
            JxlDecoderSetInput(cached_jxl_decoder.get(), cached_jxl_data, cached_jxl_data_size);
            JxlDecoderCloseInput(cached_jxl_decoder.get());
            // return true;
        }
        else {
            fprintf(stderr, "Unknown decoder status\n");
            return false;
        }
    }

}

void* JxlReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
    int& frame_count,
    int& frame_time,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
    has_animation = false;
	unsigned char* pPixelData = NULL;


    std::vector<uint8_t> pixels;
    std::vector<uint8_t> icc_profile;
    if (!DecodeJpegXlOneShot((const uint8_t*)buffer, sizebytes, &pixels, width, height,
        has_animation, frame_count, frame_time, &icc_profile, outOfMemory)) {
        fprintf(stderr, "Error while decoding the jxl file\n");
        return NULL;
    }
    int size = width * height * nchannels;
    if (pPixelData = new(std::nothrow) unsigned char[size])
        memcpy(pPixelData, pixels.data(), size);

    // RGBA -> BGRA conversion (with little-endian integers)
    for (uint32_t* i = (uint32_t*)pPixelData; (uint8_t*)i < pPixelData + size; i++)
        *i = ((*i & 0x00FF0000) >> 16) | ((*i & 0x0000FF00)) | ((*i & 0x000000FF) << 16) | ((*i & 0xFF000000));

    return pPixelData;
}

void JxlReader::DeleteCache() {
    // JxlDecoderDestroy(cached_jxl_decoder.get());
    // JxlResizableParallelRunnerDestroy(cached_jxl_runner.get());
    cached_jxl_info = { 0 };
    free(cached_jxl_data);
    cached_jxl_data = NULL;
    cached_jxl_data_size = 0;
    cached_jxl_decoder = NULL;
    cached_jxl_runner = NULL;
}