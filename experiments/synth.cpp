#ifndef LODE_IO_HEADER
#define LODE_IO_HEADER

#include "../lodepng.h"
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

	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);

	size_t repeat = 0;
	size_t matches = 0;

	for(size_t i=1;i<width*height;i++){
		if(
			   decoded[(i)*4 + 0] == decoded[(i - 1)*4 + 0]
			&& decoded[(i)*4 + 1] == decoded[(i - 1)*4 + 1]
			&& decoded[(i)*4 + 2] == decoded[(i - 1)*4 + 2]
			&& decoded[(i)*4 + 3] == decoded[(i - 1)*4 + 3]
		){
			repeat++;
			if(repeat == 4){
				matches++;
				repeat = 0;
			}
		}
	}
	printf("synth: %f\n",(double)matches*4/(width*height));
	delete[] decoded;

	return 0;
}
