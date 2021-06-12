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

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);

	uint8_t* matches = new uint8_t[width*height*4];

	size_t limit = 8192;

	for(int i=0;i<width*height;){
		size_t match_length = 0;
		for(int step_back=1;step_back < limit && i - step_back > 0;step_back++){
			for(size_t len=0;i + len < width*height;){
				if(
					   decoded[(i + len)*4 + 0] == decoded[(i - step_back + len)*4 + 0]
					&& decoded[(i + len)*4 + 1] == decoded[(i - step_back + len)*4 + 1]
					&& decoded[(i + len)*4 + 2] == decoded[(i - step_back + len)*4 + 2]
					&& decoded[(i + len)*4 + 3] == decoded[(i - step_back + len)*4 + 3]
				){
					len++;
					if(len > match_length){
						match_length = len;
					}
				}
				else{
					break;
				}
			}
		}
		if(match_length < 5){
			matches[i*4 + 0] = 0;
			matches[i*4 + 1] = 0;
			matches[i*4 + 2] = 0;
			matches[i*4 + 3] = 255;
			i++;
		}
		else{
			//printf("what\n");
			for(size_t t=0;t<match_length;t++){
				matches[i*4 + 0] = 255;
				matches[i*4 + 1] = 255;
				matches[i*4 + 2] = 255;
				matches[i*4 + 3] = 255;
				i++;
			}
		}
	}
	delete[] decoded;


	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image[i*4+0] = matches[i*4 + 0];
		image[i*4+1] = matches[i*4 + 1];
		image[i*4+2] = matches[i*4 + 2];
		image[i*4+3] = matches[i*4 + 3];
	}
	encodeOneStep("lz.png", image, width, height);



	delete[] matches;
	return 0;
}
