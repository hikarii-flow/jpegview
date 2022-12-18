#include "stdafx.h"

#include "PNGWrapper.h"
#include "png.h"
#include "MaxImageDef.h"

unsigned int    width, height, channels, rowbytes;
unsigned int    w0, h0;
png_infop       g_info_ptr;
unsigned int    cur = 0;
unsigned char* p_frame = NULL;
png_bytepp      rows = NULL;

void save_tga(unsigned char** rows, unsigned int w, unsigned int h, unsigned int channels)
{
	char szOut[512];
	FILE* f2;
	if (channels == 4)
	{
		unsigned short tgah[9] = { 0,2,0,0,0,0,(unsigned short)w,(unsigned short)h,0x0820 };
		sprintf(szOut, "test_load8_%03d.tga", cur);
		if ((f2 = fopen(szOut, "wb")) != 0)
		{
			unsigned int j;
			if (fwrite(&tgah, 1, 18, f2) != 18) return;
			for (j = 0; j < h; j++)
				if (fwrite(rows[h - 1 - j], channels, w, f2) != w) return;
			fclose(f2);
		}
		printf("  [libpng");
#ifdef PNG_APNG_SUPPORTED
		printf("+apng");
#endif
		printf(" %s]:  ", PNG_LIBPNG_VER_STRING);
		printf("%s : %dx%d     %c\n", szOut, w, h, cur > 0 ? '*' : ' ');
	}
	cur++;
}

#ifdef PNG_APNG_SUPPORTED
void frame_info_fn(png_structp png_ptr, png_uint_32 frame_num)
{
	save_tga(rows, w0, h0, channels);

	/*x0 = png_get_next_frame_x_offset(png_ptr, g_info_ptr);
	y0 = png_get_next_frame_y_offset(png_ptr, g_info_ptr);*/
	w0 = png_get_next_frame_width(png_ptr, g_info_ptr);
	h0 = png_get_next_frame_height(png_ptr, g_info_ptr);
}

/*void frame_end_fn(png_structp png_ptr, png_uint_32 frame_num)
{
  save_tga(rows, w0, h0, channels, i);
}*/
#endif

void info_fn(png_structp png_ptr, png_infop info_ptr)
{
	png_set_expand(png_ptr);
	png_set_strip_16(png_ptr);
	png_set_gray_to_rgb(png_ptr);
	png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	png_set_bgr(png_ptr);
	(void)png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	channels = png_get_channels(png_ptr, info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	w0 = width;
	h0 = height;

#ifdef PNG_APNG_SUPPORTED
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
		png_set_progressive_frame_fn(png_ptr, frame_info_fn, NULL);

	/*if (png_get_first_frame_is_hidden(png_ptr, info_ptr))
	  return;*/
#endif

	p_frame = (unsigned char*)malloc(height * rowbytes);
	rows = (png_bytepp)malloc(height * sizeof(png_bytep));

	if (p_frame && rows)
	{
		unsigned int j;

		for (j = 0; j < height; j++)
			rows[j] = p_frame + j * rowbytes;
	}
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
	png_progressive_combine_row(png_ptr, rows[row_num], new_row);
}

void end_fn(png_structp png_ptr, png_infop info_ptr)
{
	if (p_frame && rows)
	{
		save_tga(rows, w0, h0, channels);
		free(rows);
		free(p_frame);
	}
}

void load_png(const void* buffer, int sizebytes)
{
	// FILE* f1;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop   info_ptr = png_create_info_struct(png_ptr);
	if (png_ptr && info_ptr)
	{
		if (setjmp(png_jmpbuf(png_ptr)))
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return;
		}
		g_info_ptr = info_ptr;
		png_set_progressive_read_fn(png_ptr, NULL, info_fn, row_fn, end_fn);

		if (true) //(f1 = fopen(szImage, "rb")) != 0)
		{
			unsigned char smallbuf[1024];
			int len;
			int offset = 0;
			do
			{
				len = min(sizebytes - offset, 1024);
				memcpy(smallbuf, (unsigned char*)buffer + offset, len);
				offset += len;
				// len = fread(smallbuf, 1, 1024, f1);
				if (len > 0)
					png_process_data(png_ptr, info_ptr, smallbuf, len);
			} while (len == 1024 && offset < sizebytes); //!feof(f1));
			// fclose(f1);
		}

		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	}
}



void* PngReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	load_png(buffer, sizebytes);
	return NULL;
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;

	//png_image image;
	//png_image_begin_read_from_memory(&image, buffer, sizebytes);
	//image.format = PNG_FORMAT_BGRA;
	//png_bytep png_bytes;
	//png_bytes = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
	//if (!png_image_finish_read(&image, NULL, png_bytes, 0, NULL))
	//	return NULL;



	// unsigned int    width, height, depth, coltype, rowbytes, size, i, j;

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
	png_set_gray_to_rgb(read_png_ptr);                        /* make it RGB */
	png_set_add_alpha(read_png_ptr, 0xff, PNG_FILLER_AFTER);  /* make it RGBA */
	(void)png_set_interlace_handling(read_png_ptr);
	png_read_update_info(read_png_ptr, read_info_ptr);
	width = png_get_image_width(read_png_ptr, read_info_ptr);
	height = png_get_image_height(read_png_ptr, read_info_ptr);
	depth = png_get_bit_depth(read_png_ptr, read_info_ptr);
	coltype = png_get_color_type(read_png_ptr, read_info_ptr);
	rowbytes = png_get_rowbytes(read_png_ptr, read_info_ptr);
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
	return pPixelData;
}

