#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <cmath>
#include <math.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "filters.hpp"
#include "2dutils.hpp"
#include "lode_io.hpp"
#include "varint.hpp"

void print_usage(){
	printf("./choh infile.png outfile.hoh speed\n\nspeed is a number from 0-4\nCurrently greyscale only (the G component of a PNG file be used for RGB input)\n");
}

uint8_t* encode_grey_8bit_simple(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	uint8_t* out_buf = new uint8_t[width*height + 64];
	outPointer = out_buf;

	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);

	*(outPointer++) = 0b00011100;//8bit greyscale header

	*(outPointer++) = 0b00000000;//use no compression features
	for(size_t i=0;i<width*height;i++){
		*(outPointer++) = in_bytes[i];
	}

	return out_buf;
}

int main(int argc, char *argv[]){
	if(argc < 4){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	size_t speed = atoi(argv[3]);

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);

	printf("converting to greyscale\n");
	uint8_t* grey = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		grey[i] = decoded[i*4 + 1];
	}
	delete[] decoded;

	uint8_t* outPointer;
	printf("encoding as uncompressed hoh\n");
	uint8_t* out_buf = encode_grey_8bit_simple(grey,width,height,outPointer);
	
	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[2],out_buf,outPointer - out_buf);
	delete[] out_buf;

	return 0;
}
