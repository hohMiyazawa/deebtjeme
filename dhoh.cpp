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
#include "panic.hpp"

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

uint8_t* read_8bit_greyscale(uint8_t*& fileIndex,uint32_t width,uint32_t height){
	uint8_t compressionMode = *(fileIndex++);

	uint8_t RESERVED    = (compressionMode & 0b10000000) >> 7;
	uint8_t SYMMETRY    = (compressionMode & 0b01000000) >> 6;
	uint8_t SUB_GREEN   = (compressionMode & 0b00100000) >> 5;
	uint8_t INDEX       = (compressionMode & 0b00010000) >> 4;

	uint8_t PREDICTION  = (compressionMode & 0b00001000) >> 3;
	uint8_t ENTROPY_MAP = (compressionMode & 0b00000100) >> 2;
	uint8_t LZ          = (compressionMode & 0b00000010) >> 1;
	uint8_t CODED       = (compressionMode & 0b00000001) >> 0;
	if(RESERVED == 1){
		panic("reserved bit set! not a valid hoh file\n");
	}
	if(SUB_GREEN == 1){
		panic("SUB_GREEN transform not valid for greyscale images!\n");
	}
	uint8_t* bitmap;
	if(
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 0
		&& ENTROPY_MAP == 0
		&& LZ == 0
		&& CODED == 0
	){
		bitmap = new uint8_t[width*height];
		for(size_t i=0;i<width*height;i++){
			bitmap[i] = *(fileIndex++);
		}
	}
	uint8_t* bitmap_expanded = new uint8_t[width*height*4];
	for(size_t i=0;i<width*height;i++){
		bitmap_expanded[i*4 + 0] = bitmap[i];
		bitmap_expanded[i*4 + 1] = bitmap[i];
		bitmap_expanded[i*4 + 2] = bitmap[i];
		bitmap_expanded[i*4 + 3] = 255;
	}
	delete[] bitmap;
	return bitmap_expanded;
}

uint8_t* readImage(uint8_t*& fileIndex,uint32_t width,uint32_t height){
	uint8_t imageMode = *(fileIndex++);
	uint8_t reservedBit = imageMode >> 7;
	if(reservedBit == 1){
		panic("reserved bit set! not a valid hoh file\n");
	}
	uint8_t bitDepth = ((imageMode & 0b01111100) >> 2) + 1;
	if(bitDepth != 8){
		panic("bit depths other than 8 not yet implemented!\n");
	}
	uint8_t hasAlpha  = (imageMode & 0b00000010) >> 1;
	uint8_t hasColour = (imageMode & 0b00000001);
	if(hasAlpha){
		panic("alpha decoding not yet implemented!\n");
	}
	if(hasColour){
		panic("colour decoding not yet implemented!\n");
	}
	if(!hasAlpha && !hasColour && bitDepth == 8){
		return read_8bit_greyscale(fileIndex,width,height);
	}
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
