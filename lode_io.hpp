#ifndef LODE_IO_HEADER
#define LODE_IO_HEADER

#include "lodepng.h"
#include "panic.hpp"
#include <iostream>

void encodeOneStep(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height) {
	//Encode the image
	unsigned error = lodepng::encode(filename, image, width, height);

	//if there's an error, display it
	if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
}

uint8_t* decodeOneStep(const char* filename,unsigned* width_out,unsigned* height_out) {
	std::vector<unsigned char> image; //the raw pixels
	unsigned width, height;

	//decode
	unsigned error = lodepng::decode(image, width, height, filename);
	if(error){
		panic("loading error\n");
	}

	*width_out = width;
	*height_out = height;

	//if there's an error, display it
	if(error){
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
	}


	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
	uint8_t* a = new uint8_t[width*height*4];
	std::copy(image.begin(), image.end(), a);

	return a;
}

#endif //LODE_IO_HEADER
