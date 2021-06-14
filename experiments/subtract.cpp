#include "../lode_io.hpp"

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		return 1;
	}

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);
	uint8_t* decoded2 = decodeOneStep(argv[2],&width,&height);

	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image[i*4+0] = (decoded[i*4+0] - decoded2[i*4+0] + 256) % 256;
		image[i*4+1] = (decoded[i*4+1] - decoded2[i*4+1] + 256) % 256;
		image[i*4+2] = (decoded[i*4+2] - decoded2[i*4+2] + 256) % 256;
		image[i*4+3] = 255;
	}
	encodeOneStep("subtract.png", image, width, height);
	delete[]  decoded;
	delete[]  decoded2;
	return 0;
}
