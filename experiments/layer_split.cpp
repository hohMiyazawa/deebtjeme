#include "../lode_io.hpp"

/*
Clips tops of red_green and blue_green channels!!!

A visual tool only
*/

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

	uint8_t* red = new uint8_t[width*height];
	uint8_t* green = new uint8_t[width*height];
	uint8_t* blue = new uint8_t[width*height];
	uint8_t* red_g = new uint8_t[width*height];
	uint8_t* blue_g = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		red[i] =   decoded[i*4 + 0];
		green[i] = decoded[i*4 + 1];
		blue[i] =  decoded[i*4 + 2];
	}
	for(size_t i=0;i<width*height;i++){
		int test = (int)decoded[i*4 + 0] - (int)decoded[i*4 + 1] + 128;
		if(test < 0){
			test = 0;
		}
		if(test > 255){
			test = 255;
		}
		red_g[i] = (uint8_t)test;
	}
	for(size_t i=0;i<width*height;i++){
		int test = (int)decoded[i*4 + 2] - (int)decoded[i*4 + 1] + 128;
		if(test < 0){
			test = 0;
		}
		if(test > 255){
			test = 255;
		}
		blue_g[i] = (uint8_t)test;
	}
	delete[] decoded;


	std::vector<unsigned char> image_green;
	image_green.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_green[i*4+0] = green[i];
		image_green[i*4+1] = green[i];
		image_green[i*4+2] = green[i];
		image_green[i*4+3] = 255;
	}
	encodeOneStep("green.png", image_green, width, height);


	std::vector<unsigned char> image_red;
	image_red.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_red[i*4+0] = red[i];
		image_red[i*4+1] = red[i];
		image_red[i*4+2] = red[i];
		image_red[i*4+3] = 255;
	}
	encodeOneStep("red.png", image_red, width, height);


	std::vector<unsigned char> image_blue;
	image_blue.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_blue[i*4+0] = blue[i];
		image_blue[i*4+1] = blue[i];
		image_blue[i*4+2] = blue[i];
		image_blue[i*4+3] = 255;
	}
	encodeOneStep("blue.png", image_blue, width, height);

	std::vector<unsigned char> image_red_g;
	image_red_g.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_red_g[i*4+0] = red_g[i];
		image_red_g[i*4+1] = red_g[i];
		image_red_g[i*4+2] = red_g[i];
		image_red_g[i*4+3] = 255;
	}
	encodeOneStep("red_g.png", image_red_g, width, height);


	std::vector<unsigned char> image_blue_g;
	image_blue_g.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image_blue_g[i*4+0] = blue_g[i];
		image_blue_g[i*4+1] = blue_g[i];
		image_blue_g[i*4+2] = blue_g[i];
		image_blue_g[i*4+3] = 255;
	}
	encodeOneStep("blue_g.png", image_blue_g, width, height);

	delete[] red;
	delete[] red_g;
	delete[] green;
	delete[] blue;
	delete[] blue_g;
	return 0;
}
