#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "2dutils.hpp"
#include "varint.hpp"
#include "lode_io.hpp"

void print_usage(){
	printf("./decoder infile.hoh outfile.png\n");
}

uint8_t* readImage(uint8_t* fileIndex,uint32_t width,uint32_t height){
}

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}
	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	printf("read %d bytes\n",(int)in_size);

	uint8_t* fileIndex = in_bytes;
	uint32_t width =    readVarint(fileIndex) + 1;
	uint32_t height =   readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* out_bytes = readImage(fileIndex,width,height);
	
	delete[] in_bytes;

	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height*4;i++){
		image[i] = out_bytes[i];
	}

	delete[] out_bytes;

	encodeOneStep(argv[2], image, width, height);

	return 0;
}
