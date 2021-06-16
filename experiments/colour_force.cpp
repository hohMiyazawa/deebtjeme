#ifndef LODE_IO_HEADER
#define LODE_IO_HEADER

#include "../lodepng.h"
#include "../delta_colour.hpp"
#include "../colour_filters.hpp"
#include "../entropy_estimation.hpp"
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

	*width_out = width;
	*height_out = height;

	//if there's an error, display it
	if(error){
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
	}


	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
	uint8_t* a = new uint8_t[width*height*8];
	std::copy(image.begin(), image.end(), a);

	return a;
}

#endif //LODE_IO_HEADER

int main(int argc, char *argv[]){
	if(argc < 2){
		printf("not enough arguments\n");
		return 1;
	}

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);

	double best = 0;

	for(size_t g=0;g<256;g++){
		uint16_t rsub[width*height];
		for(size_t i=0;i<width*height;i++){
			rsub[i] = 256 + decoded[i*4 + 1] - delta(g,decoded[i*4]);
		}
		uint8_t* residual = hbd_filter_all_ffv1(rsub, 256, width, height);
		SymbolStats stats;
		stats.count_freqs(residual, width*height);
		delete[] residual;
		double waffle = estimateEntropy_freq(stats, width*height);
		if(best == 0 || waffle < best){
			printf("%d   %f\n",(int)g,waffle);
			best = waffle;
		}
		else{
			printf("%d %f\n",(int)g,waffle);
		}
	}

	delete[] decoded;

	return 0;
}
