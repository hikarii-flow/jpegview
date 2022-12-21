#include "stdafx.h"

#include "PNGWrapper.h"
#include "MaxImageDef.h"
#include <vector>

/*
 * Modified from "load4apng.c"
 * Original: https://sourceforge.net/projects/apng/files/libpng/examples/
 *
 * loads APNG file, caches all frames to memory (32bpp).
 * including frames composition.
 *
 * needs apng-patched libpng.
 *
 * Copyright (c) 2012-2014 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png.h"

struct framedata
{
	void* pixels;
	// size_t size;
	unsigned short  delay_num;
	unsigned short  delay_den;
	unsigned int    width;
	unsigned int    height;
	unsigned int    channels;
};

std::vector<framedata> frames;

void save_tga(unsigned char** rows, unsigned int w, unsigned int h, unsigned int channels, unsigned int frame, unsigned short delay_num, unsigned short delay_den)
{
	char szOut[512];
	// FILE* f2;
	if (channels == 4)
	{
		unsigned short tgah[9] = { 0,2,0,0,0,0,(unsigned short)w,(unsigned short)h,0x0820 };
		sprintf(szOut, "test_load4_%03d.tga", frame);
		if (true) //(f2 = fopen(szOut, "wb")) != 0)
		{
			unsigned int j;
			struct framedata fd;
			fd.delay_num = delay_num;
			fd.delay_den = delay_den;
			fd.pixels = malloc(w * h * channels);
			if (fd.pixels == NULL)
				return; // TODO
			fd.width = w;
			fd.height = h;
			fd.channels = channels;
			if (!fd.pixels)
				return;
			// if (fwrite(&tgah, 1, 18, f2) != 18) return;
			for (j = 0; j < h; j++) {
				memcpy((char*)fd.pixels + j * w * channels, rows[j], w * channels);
				// if (fwrite(rows[h - 1 - j], channels, w, f2) != w) return;
			}
			frames.push_back(fd);
			// fclose(f2);
		}
		printf("  [libpng");
#ifdef PNG_APNG_SUPPORTED
		printf("+apng");
#endif
		printf(" %s]:  ", PNG_LIBPNG_VER_STRING);
		printf("%s : %dx%d   %c\n", szOut, w, h, frame > 0 ? '*' : ' ');
	}
}

#ifdef PNG_APNG_SUPPORTED
void BlendOver(unsigned char** rows_dst, unsigned char** rows_src, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	unsigned int  i, j;
	int u, v, al;

	for (j = 0; j < h; j++)
	{
		unsigned char* sp = rows_src[j];
		unsigned char* dp = rows_dst[j + y] + x * 4;

		for (i = 0; i < w; i++, sp += 4, dp += 4)
		{
			if (sp[3] == 255)
				memcpy(dp, sp, 4);
			else
				if (sp[3] != 0)
				{
					if (dp[3] != 0)
					{
						u = sp[3] * 255;
						v = (255 - sp[3]) * dp[3];
						al = u + v;
						dp[0] = (sp[0] * u + dp[0] * v) / al;
						dp[1] = (sp[1] * u + dp[1] * v) / al;
						dp[2] = (sp[2] * u + dp[2] * v) / al;
						dp[3] = al / 255;
					}
					else
						memcpy(dp, sp, 4);
				}
		}
	}
}
#endif

struct iterData {
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 w0;
	png_uint_32 h0;
	png_uint_32 x0;
	png_uint_32 y0;
	unsigned short delay_num;
	unsigned short delay_den;
	unsigned char dop;
	unsigned char bop;
	unsigned int first;
	png_bytepp rows_image;
	png_bytepp rows_frame;
	unsigned char* p_image;
	unsigned char* p_frame;
	unsigned char* p_temp;
	unsigned int size;
	unsigned int width;
	unsigned int height;
	unsigned int channels;
	png_uint_32 frame_count;
	
};

struct iterData env;

void doStuff(png_structp& png_ptr, png_infop& info_ptr, png_uint_32& w0, png_uint_32& h0, png_uint_32& x0, png_uint_32& y0,
	unsigned short& delay_num, unsigned short& delay_den, unsigned char& dop, unsigned char& bop, unsigned int& first,
	png_bytepp& rows_image, png_bytepp& rows_frame, unsigned char* p_image, unsigned char* p_temp, unsigned int& size,
	unsigned int& width, unsigned int& height, unsigned int& channels, unsigned int i)
{
	unsigned int j;
	#ifdef PNG_APNG_SUPPORTED
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
		{
			png_read_frame_head(png_ptr, info_ptr);
			png_get_next_frame_fcTL(png_ptr, info_ptr, &w0, &h0, &x0, &y0, &delay_num, &delay_den, &dop, &bop);
		}
		if (i == first)
		{
			bop = PNG_BLEND_OP_SOURCE;
			if (dop == PNG_DISPOSE_OP_PREVIOUS)
				dop = PNG_DISPOSE_OP_BACKGROUND;
		}
	#endif
		png_read_image(png_ptr, rows_frame);

	#ifdef PNG_APNG_SUPPORTED
		if (dop == PNG_DISPOSE_OP_PREVIOUS)
			memcpy(p_temp, p_image, size);

		if (bop == PNG_BLEND_OP_OVER)
			BlendOver(rows_image, rows_frame, x0, y0, w0, h0);
		else
	#endif
			for (j = 0; j < h0; j++)
				memcpy(rows_image[j + y0] + x0 * 4, rows_frame[j], w0 * 4);

		save_tga(rows_image, width, height, channels, i, delay_num, delay_den);

	#ifdef PNG_APNG_SUPPORTED
		if (dop == PNG_DISPOSE_OP_PREVIOUS)
			memcpy(p_image, p_temp, size);
		else
			if (dop == PNG_DISPOSE_OP_BACKGROUND)
				for (j = 0; j < h0; j++)
					memset(rows_image[j + y0] + x0 * 4, 0, w0 * 4);
	#endif
}

int offset = 0;
void read_data_fn(png_structp png_ptr, png_bytep outbuffer, png_size_t sizebytes)
{
	png_voidp io_ptr = png_get_io_ptr(png_ptr);
	if (io_ptr == NULL)
		return;   // add custom error handling here

	memcpy(outbuffer, (char*)io_ptr + offset, sizebytes);
	offset += sizebytes;
}

png_uint_32 load_png(void* buffer, size_t sizebytes, bool& outOfMemory)
{
	//FILE* f1;

	if (true) //(f1 = fopen(szImage, "rb")) != 0)
	{
		unsigned int    width, height, channels, rowbytes, size, i, j;
		png_bytepp      rows_image;
		png_bytepp      rows_frame;
		unsigned char*  p_image;
		unsigned char*  p_frame;
		unsigned char*  p_temp;
		unsigned char   sig[8];

		if (true) //fread(sig, 1, 8, f1) == 8 && png_sig_cmp(sig, 0, 8) == 0)
		{
			png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			png_infop   info_ptr = png_create_info_struct(png_ptr); //TODO cleanup png_ptr if this fails
			if (png_ptr && info_ptr)
			{
				if (setjmp(png_jmpbuf(png_ptr)))
				{
					png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
					// fclose(f1);
					return 0;
				}
				// png_init_io(png_ptr, f1);
				png_set_sig_bytes(png_ptr, 8);
				png_set_read_fn(png_ptr, (char*)buffer+8, read_data_fn);
				png_read_info(png_ptr, info_ptr);
				png_set_expand(png_ptr);
				png_set_strip_16(png_ptr);
				png_set_gray_to_rgb(png_ptr);
				png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
				png_set_bgr(png_ptr);
				(void)png_set_interlace_handling(png_ptr);
				png_read_update_info(png_ptr, info_ptr);
				width = png_get_image_width(png_ptr, info_ptr);
				height = png_get_image_height(png_ptr, info_ptr);
				if (abs((double)width * height) > MAX_IMAGE_PIXELS) {
					png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
					// fclose(f1);
					outOfMemory = true;
					return 0;
				}
				channels = png_get_channels(png_ptr, info_ptr);
				rowbytes = png_get_rowbytes(png_ptr, info_ptr);
				size = height * rowbytes;
				p_image = (unsigned char*)malloc(size);
				p_frame = (unsigned char*)malloc(size);
				p_temp = (unsigned char*)malloc(size);
				rows_image = (png_bytepp)malloc(height * sizeof(png_bytep));
				rows_frame = (png_bytepp)malloc(height * sizeof(png_bytep));
				if (p_image && p_frame && p_temp && rows_image && rows_frame)
				{
					png_uint_32     frames = 1;
					png_uint_32     x0 = 0;
					png_uint_32     y0 = 0;
					png_uint_32     w0 = width;
					png_uint_32     h0 = height;
#ifdef PNG_APNG_SUPPORTED
					png_uint_32     plays = 0;
					unsigned short  delay_num = 1;
					unsigned short  delay_den = 10;
					unsigned char   dop = 0;
					unsigned char   bop = 0;
					unsigned int    first = (png_get_first_frame_is_hidden(png_ptr, info_ptr) != 0) ? 1 : 0;
					if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
						png_get_acTL(png_ptr, info_ptr, &frames, &plays);
#endif
					for (j = 0; j < height; j++)
						rows_image[j] = p_image + j * rowbytes;

					for (j = 0; j < height; j++)
						rows_frame[j] = p_frame + j * rowbytes;

					env.bop = bop;
					env.channels = channels;
					env.delay_den = delay_den;
					env.delay_num = delay_num;
					env.dop = dop;
					env.first = first;
					env.h0 = h0;
					env.height = height;
					env.info_ptr = info_ptr;
					env.png_ptr = png_ptr;
					env.p_image = p_image;
					env.p_frame = p_frame;
					env.p_temp = p_temp;
					env.rows_frame = rows_frame;
					env.rows_image = rows_image;
					env.size = size;
					env.w0 = w0;
					env.width = width;
					env.x0 = x0;
					env.y0 = y0;
					env.frame_count = frames;
					return frames;
				}
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			}
		}
	}
	return 0;
}


/*                    for (i = 0; i < frames; i++)
                    {
                        doStuff();
                    }*/

bool unload_png(png_structp& png_ptr, png_infop& info_ptr, png_bytepp& rows_frame, png_bytepp& rows_image,
	unsigned char* p_temp, unsigned char* p_frame, unsigned char* p_image) 
{
	png_read_end(png_ptr, info_ptr);
	free(rows_frame);
	free(rows_image);
	free(p_temp);
	free(p_frame);
	free(p_image);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return true;
}

unsigned int counter = 0;
void* last = NULL;
FILE* cached_file = NULL;

void* PngReader::ReadImage(int& width2,
	int& height2,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	FILE* file,
	void* buffer,
	size_t sizebytes)
{
	// fmemopen(buffer, sizebytes, )
	if (frames.size() == 0 && (!load_png(buffer, sizebytes, outOfMemory))) {
		cached_file = NULL;
		return NULL;
	}
	if (frames.size() < env.frame_count) {
		doStuff(env.png_ptr, env.info_ptr, env.w0, env.h0, env.x0, env.y0, env.delay_num, env.delay_den, env.dop, env.bop, env.first,
			env.rows_image, env.rows_frame, env.p_image, env.p_temp, env.size, env.width, env.height, env.channels, counter);
	} else if (cached_file) {
		unload_png(env.png_ptr, env.info_ptr, env.rows_frame, env.rows_image, env.p_temp, env.p_frame, env.p_image);
		fclose(cached_file);
		cached_file = NULL;
	}
	frame_count = env.frame_count;
	struct framedata fd = frames.at(min(frames.size() - 1, counter % frame_count));
	counter++;
	width2 = fd.width;
	height2 = fd.height;
	nchannels = fd.channels;
	
	has_animation = (frame_count > 1);

	// https://wiki.mozilla.org/APNG_Specification
	// "If the denominator is 0, it is to be treated as if it were 100"
	if (!fd.delay_den)
		fd.delay_den = 100; 
	// "If the the value of the numerator is 0 the decoder should render the next frame as quickly as possible"
	frame_time = max((int)(1000.0 * fd.delay_num / fd.delay_den), 1);

	unsigned char* pPixelData = NULL;
	pPixelData = new(std::nothrow) unsigned char[fd.width * fd.height * fd.channels];
	if (pPixelData)
		memcpy(pPixelData, fd.pixels, fd.width * fd.height * fd.channels);
	
	return pPixelData;

	/*
	dst = NULL;
	load_png(buffer, sizebytes);
	width2 = w0;
	height2 = h0;
	has_animation = false;
	nchannels = 4;
	frame_count = 1;
	
	return dst;
	return NULL;
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;
	*/

	//png_image image;
	//png_image_begin_read_from_memory(&image, buffer, sizebytes);
	//image.format = PNG_FORMAT_BGRA;
	//png_bytep png_bytes;
	//png_bytes = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
	//if (!png_image_finish_read(&image, NULL, png_bytes, 0, NULL))
	//	return NULL;



	// unsigned int    width, height, depth, coltype, rowbytes, size, i, j;
	/*
	unsigned int    depth, coltype, rowbytes, size, i, j;
	png_bytepp      rows;
	unsigned char* p_frame;
	unsigned char   sig[8];
	memcpy(sig, buffer, 8);
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	
	if (!png_ptr)
		return NULL;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return NULL;
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}
	png_structp read_png_ptr = png_ptr;
	png_infop read_info_ptr = info_ptr;
	png_set_progressive_read_fn(png_ptr, NULL, info_fn, row_fn, end_fn);

	png_process_data(png_ptr, info_ptr, (png_bytep)buffer, sizebytes);
	png_set_sig_bytes(read_png_ptr, 8);
	png_read_info(read_png_ptr, read_info_ptr);
	png_set_expand(read_png_ptr);
	png_set_strip_16(read_png_ptr);
	//png_set_gray_to_rgb(read_png_ptr);                        /* make it RGB */
	//png_set_add_alpha(read_png_ptr, 0xff, PNG_FILLER_AFTER);  /* make it RGBA */
	/*(void)png_set_interlace_handling(read_png_ptr);
	png_read_update_info(read_png_ptr, read_info_ptr);
	width = png_get_image_width(read_png_ptr, read_info_ptr);
	height = png_get_image_height(read_png_ptr, read_info_ptr);
	depth = png_get_bit_depth(read_png_ptr, read_info_ptr);
	coltype = png_get_color_type(read_png_ptr, read_info_ptr);
	rowbytes = png_get_rowbytes(read_png_ptr, read_info_ptr);
	*/
	/*
	png_process_data(png_ptr, info_ptr, (png_bytep)buffer, sizebytes);
	frame_count = png_get_num_frames(png_ptr, info_ptr);
	if (frame_count < 1)
		return NULL;
	png_read_frame_head(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	//png_read_info()
	//png_read_info();
	//png_read_frame_head(image, )
	*/
	// return pPixelData;
}

void PngReader::DeleteCache() {
	// JxlDecoderDestroy(cached_jxl_decoder.get());
	// JxlResizableParallelRunnerDestroy(cached_jxl_runner.get());
	if (cached_file) {
		unload_png(env.png_ptr, env.info_ptr, env.rows_frame, env.rows_image, env.p_temp, env.p_frame, env.p_image);
		fclose(cached_file);
		cached_file = NULL;
	}
	for (struct framedata fd : frames) {
		free(fd.pixels);
	}
	frames.clear();
}