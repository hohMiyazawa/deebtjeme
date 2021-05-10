#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"

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

SymbolStats readTable(uint8_t*& fileIndex,size_t range){
	uint8_t mode = *(fileIndex++);
	SymbolStats stats;
	for(size_t i=0;i<256;i++){
		stats.freqs[i] = 0;
	}
	printf("table mode: %d\n",(int)mode);
	if(mode == 0){
		for(size_t i=0;i<range;i++){
			stats.freqs[i] = 1;
		}
	}
	else if(mode == 8){
		for(size_t i=0;i<range;i++){
			stats.freqs[i] = *(fileIndex++);
		}
	}
	else if(mode == 16){
		for(size_t i=0;i<range;i++){
			uint32_t lower = *(fileIndex++);
			uint32_t upper = *(fileIndex++);
			stats.freqs[i] = (lower << 8) + upper;
		}
	}
	stats.normalize_freqs(1 << 16);
	printf("table read!\n");
	return stats;
}

uint8_t* readImageData_simple(
	uint8_t*& fileIndex,
	uint32_t width,
	uint32_t height,
	size_t range
){
	printf("simple image started!\n");
	SymbolStats stats = readTable(fileIndex,range);
	uint8_t* buffer = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		buffer[i] = 0;
	}
	uint8_t cum2sym[1 << 16];
	for (int s=0; s < 256; s++){
		for (uint32_t i=stats.cum_freqs[s]; i < stats.cum_freqs[s+1]; i++){
			cum2sym[i] = s;
		}
	}

	RansDecSymbol dsyms[range];
	for(size_t i=0;i<range;i++){
	        RansDecSymbolInit(&dsyms[i], stats.cum_freqs[i], stats.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t s = cum2sym[RansDecGet(&rans, 16)];
		buffer[i] = (uint8_t)s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	printf("simple image read!\n");
	return buffer;
}

uint8_t* readImageData_1prediction_entropy(
	uint8_t*& fileIndex,
	uint32_t width,
	uint32_t height,
	size_t range,
	uint8_t predictor,
	uint8_t contexts,
	uint8_t* entropyImage,
	uint32_t e_width,
	uint32_t e_height,
	uint16_t contextBlockSize
){
	SymbolStats stats[contexts];
	for(size_t i=0;i<contexts;i++){
		stats[i] = readTable(fileIndex,range);
	}
	uint8_t* buffer = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		buffer[i] = 0;
	}
	return buffer;
}

uint8_t* readImage(uint8_t*& fileIndex,uint32_t width,uint32_t height,size_t range){
	uint8_t modebyte1 = *(fileIndex++);
	uint8_t usePrediction = modebyte1 & 0b10000000;
	uint8_t useEntropy =    modebyte1 & 0b01000000;
	uint8_t useLZ =         modebyte1 & 0b00100000;

	uint8_t predictors[235];
	uint8_t predictorLength = 0;
	uint8_t contexts = 0;
	uint16_t contextBlockSize = 0;
	uint32_t e_width;
	uint32_t e_height;
	uint8_t* entropyImage;
	for(size_t i=0;i<235;i++){
		predictors[i] = 0;
	}
	if(usePrediction){
		printf("  prediction\n");
		uint8_t codeword = modebyte1 % 32;
		if(codeword == 0){
			predictors[0] = 0;
			predictorLength = 1;
		}
		else{
			printf("NO IMPLEMENTATION!\n");
		}
		if(codeword > 1){
		}
		if(useEntropy){
			printf("  entropy image\n");
			uint8_t modebyte2 = *(fileIndex++);
			contexts = modebyte2;
			uint8_t modebyte3 = *(fileIndex++);
			uint16_t scale = modebyte3 >> 5;
			uint16_t sub = modebyte3 & 0b00010000;
			contextBlockSize = (1 << (1 + scale)) * (2 + sub);
			e_width = (width + contextBlockSize - 1)/contextBlockSize;
			e_height = (height + contextBlockSize - 1)/contextBlockSize;
			entropyImage = readImage(
				fileIndex,
				e_width,
				e_height,
				contexts
			);
		}
	}
	else if(useEntropy){
		printf("  entropy image\n");
		uint8_t modebyte2 = *(fileIndex++);
		contexts = ((modebyte1 % 32) << 3) + (modebyte2 >> 5);
		uint16_t scale = (modebyte2 >> 2) % 8;
		uint16_t sub = modebyte2 & 0b00000010;
		contextBlockSize = (1 << (1 + scale)) * (2 + sub);
		e_width = (width + contextBlockSize - 1)/contextBlockSize;
		e_height = (height + contextBlockSize - 1)/contextBlockSize;
	}
	else if(useLZ){
		printf("  LZ coding\n");
		printf("NO IMPLEMENTATION!\n");
	}
	if(!usePrediction && !useEntropy && !useLZ){
		return readImageData_simple(
			fileIndex,
			width,
			height,
			range
		);
	}
	else if(usePrediction && useEntropy && predictorLength == 1 && !useLZ){
		return readImageData_1prediction_entropy(
			fileIndex,
			width,
			height,
			range,
			predictors[0],
			contexts,
			entropyImage,
			e_width,
			e_height,
			contextBlockSize
		);
		delete[] entropyImage;
	}
	else{
		uint8_t* buffer = new uint8_t[width*height];
		for(size_t i=0;i<width*height;i++){
			buffer[i] = 0;
		}
		return buffer;
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

	uint8_t* fileIndex = in_bytes;
	uint32_t width =    readVarint(fileIndex) + 1;
	uint32_t height =   readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* out_bytes = readImage(fileIndex,width,height,256);
	
	delete[] in_bytes;
	delete[] out_bytes;
	return 0;
}
