#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "rans_byte.h"
#include "file_io.hpp"

void print_usage(){
	printf("./decoder infile.hoh outfile.grey speed\n");
}

uint32_t readVarint(uint8_t*& pointer){
	uint32_t value = 0;
	uint32_t read1 = *(pointer++);
	if(read1 < 128){
		return read1;
	}
	else{
		uint32_t read2 = *(pointer++);
		if(read2 < 128){
			return (read1 % 128) + (read2 << 7);
		}
		else{
			uint32_t read3 = *(pointer++);
			if(read3 < 128){
				return (read1 % 128) + ((read2 % 128) << 7)  + (read3 << 14);
			}
			else{
				uint32_t read4 = *(pointer++);
				return (read1 % 128) + ((read2 % 128) << 7)  + ((read3 % 128) << 14) + (read4 << 21);
			}
		}
	}
}

uint8_t* readImage(uint8_t*& fileIndex,uint32_t width,uint32_t height){
	uint8_t modebyte1 = *(fileIndex);
	uint8_t usePrediction = modebyte1 & 0b10000000;
	uint8_t useEntropy =    modebyte1 & 0b01000000;
	uint8_t useLZ =         modebyte1 & 0b00100000;
	if(usePrediction){
		printf("  prediction\n");
	}
	if(useEntropy){
		printf("  entropy image\n");
	}
	if(useLZ){
		printf("  LZ coding\n");
	}
	uint8_t* buffer = new uint8_t[width*height];
	return buffer;
}

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}
	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);

	uint8_t* fileIndex = in_bytes;
	uint32_t width =    readVarint(fileIndex) + 1;
	uint32_t height =   readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* out_bytes = readImage(fileIndex,width,height);
	
	delete[] in_bytes;
	delete[] out_bytes;
	return 0;
}
