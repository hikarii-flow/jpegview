
#pragma once

class PngReader
{
public:
	// Returns data in the form BGRBGR**********BGR000 where the zeros are padding to 4 byte boundary
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& has_animation,     // if the image is animated
		int& frame_count, // number of frames
		int& frame_time, // frame duration in milliseconds
		bool& outOfMemory, // set to true when no memory to read image
		int frame_index, 
		void* buffer, // memory address containing jpeg compressed data.
		size_t sizebytes); // size of jpeg compressed data

	static void DeleteCache();
};
