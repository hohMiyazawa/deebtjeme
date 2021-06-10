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
	unsigned error = lodepng::decode(image, width, height, filename, LCT_RGBA, 16);

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

	uint8_t* upper = new uint8_t[width*height*4];
	uint8_t* lower = new uint8_t[width*height*4];

	for(size_t i=0;i<width*height;i++){
		upper[i*4 + 0] = decoded[i*8 + 0];
		upper[i*4 + 1] = decoded[i*8 + 2];
		upper[i*4 + 2] = decoded[i*8 + 4];
		upper[i*4 + 3] = decoded[i*8 + 6];
		lower[i*4 + 0] = decoded[i*8 + 1];
		lower[i*4 + 1] = decoded[i*8 + 3];
		lower[i*4 + 2] = decoded[i*8 + 5];
		lower[i*4 + 3] = decoded[i*8 + 7];
	}
	delete[] decoded;


	std::vector<unsigned char> image_upper;
	image_upper.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_upper[i*4+0] = upper[i*4 + 0];
		image_upper[i*4+1] = upper[i*4 + 1];
		image_upper[i*4+2] = upper[i*4 + 2];
		image_upper[i*4+3] = upper[i*4 + 3];
	}
	encodeOneStep("upper.png", image_upper, width, height);


	std::vector<unsigned char> image_lower;
	image_lower.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_lower[i*4+0] = lower[i*4 + 0];
		image_lower[i*4+1] = lower[i*4 + 1];
		image_lower[i*4+2] = lower[i*4 + 2];
		image_lower[i*4+3] = lower[i*4 + 3];
	}
	encodeOneStep("lower.png", image_lower, width, height);



	delete[] upper;
	delete[] lower;
	return 0;
}
