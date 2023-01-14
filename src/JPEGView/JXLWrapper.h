
#pragma once

#include <vector>

class JxlReader
{
public:
	// Returns data in 4 byte BGRA
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& has_animation,     // if the image is animated
		int& frame_count, // number of frames
		int& frame_time, // frame duration in milliseconds
		int& loop_count, // number of loops
		bool& outOfMemory, // set to true when no memory to read image
		const void* buffer, // memory address containing jxl compressed data.
		int sizebytes); // size of jxl compressed data

	static void DeleteCache();

private:
	struct jxl_cache;
	static jxl_cache cache;
	static bool ReadNextFrame(const uint8_t* jxl, size_t size, std::vector<uint8_t>* pixels, int& xsize, int& ysize,
		bool& have_animation, int& frame_count, int& frame_time, int& loop_count, std::vector<uint8_t>* icc_profile, bool& outOfMemory);
};
