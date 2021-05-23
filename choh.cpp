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
#include "laplace.hpp"
#include "numerics.hpp"

void print_usage(){
	printf("./choh infile.png outfile.hoh speed\n\nspeed is a number from 0-4\nCurrently greyscale only (the G component of a PNG file be used for RGB input)\n");
}

class EntropyEncoder{
	public:
		EntropyEncoder();
		~EntropyEncoder();
		void encodeSymbol(RansEncSymbol* magic,uint8_t byte);
		uint8_t* conclude(size_t* streamSize);
		void writeByte(uint8_t byte);
	private:
		static const size_t out_max_size = 32<<20;
		static const size_t out_max_elems = out_max_size / sizeof(uint8_t);
		uint8_t* out_buf;
		uint8_t* out_end;
		uint8_t* ptr;
		RansState rans;
};

EntropyEncoder::EntropyEncoder(void){
	out_buf = new uint8_t[out_max_elems];
	out_end = out_buf + out_max_elems;
	ptr = out_end;
	RansEncInit(&rans);
}
EntropyEncoder::~EntropyEncoder(void){
	delete[] out_buf;
}
void EntropyEncoder::encodeSymbol(RansEncSymbol* magic,uint8_t byte){
	RansEncPutSymbol(&rans, &ptr, magic + byte);
}
void EntropyEncoder::writeByte(uint8_t byte){
	*ptr = byte;
	ptr--;
}
uint8_t* EntropyEncoder::conclude(size_t* streamSize){
	RansEncFlush(&rans, &ptr);
	*streamSize = out_end - ptr;
	uint8_t* out = new uint8_t[*streamSize];
	for(size_t i=0;i<*streamSize;i++){
		out[i] = ptr[i];
	}
	return out;
}

class BitBuffer{
	public:
		uint8_t buffer[1024];
		uint8_t length;
		BitBuffer();
		void writeBits(uint8_t value,uint8_t size);
		void conclude();
		
	private:
		uint8_t partial;
		uint8_t partial_length;
};

BitBuffer::BitBuffer(){
	length = 0;
	partial = 0;
	partial_length = 0;
}
void BitBuffer::conclude(){
	buffer[length++] = partial << (8 - partial_length - 1);
	partial = 0;
	partial_length = 0;
}
void BitBuffer::writeBits(uint8_t value,uint8_t size){
	if(size == 0){
	}
	else if(size + partial_length == 8){
		buffer[length++] = partial + value;
		partial = 0;
		partial_length = 0;
	}
	else if(size + partial_length < 8){
		partial += value << (8 - partial_length - size);
		partial_length += size;
	}
	else{
		buffer[length++] = partial + (value >> (partial_length + size - 8));
		partial = (value % (1 << (partial_length + size - 8))) << (16 - partial_length - size);
		partial_length = (partial_length + size - 8);
	}
}

SymbolStats encode_freqTable(SymbolStats freqs,BitBuffer* sink){
/*
	//current behaviour: always use 4bit magnitude coding
	(*sink).writeBits(0,1);
	(*sink).writeBits(2,2);

	//current behaviour: scale down to nearest power of 2
	freqs.normalize_freqs(1 << 16);
	SymbolStats newFreqs;
	for(size_t i=0;i<256;i++){
		uint32_t power = 1;
		uint32_t power_index = 1;
		while(power <= freqs.freqs[i]){
			power*=2;
			power_index++;
		}
		power = power >> 1;
		power_index--;
		if(power_index > 15){
			power_index = 15;
			power = (1 << 14);
		}
		newFreqs.freqs[i] = power;
		//printf("----%d %d %d\n",(int)i,(int)newFreqs.freqs[i],(int)freqs.freqs[i]);
		//printf("power index %d\n",(int)power_index);
		(*sink).writeBits(power_index,4);
		uint8_t extraBits = power_index >> 1;
		if(extraBits){
			extraBits--;
			if(extraBits){
				(*sink).writeBits(0,extraBits);
			}
		}
	}
*/

	//current behaviour: always use accurate 4bit magnitude coding, even if less accurate tables are sometimes more compact
	(*sink).writeBits(0,1);
	(*sink).writeBits(3,2);

	size_t sum = 0;
	for(size_t i=0;i<256;i++){
		sum += freqs.freqs[i];
	}
	if(sum > (1 << 16)){
		freqs.normalize_freqs(1 << 16);
	}
	SymbolStats newFreqs;
	for(size_t i=0;i<256;i++){
		newFreqs.freqs[i] = freqs.freqs[i];
	}
	for(size_t i=0;i<256;i++){
		uint8_t magnitude = log2_plus(freqs.freqs[i]);
		if(magnitude < 15){
			(*sink).writeBits(magnitude,4);
		}
		else if(magnitude == 15){
			(*sink).writeBits(15,4);
			(*sink).writeBits(0,1);
		}
		else if(magnitude == 16){
			(*sink).writeBits(15,4);
			(*sink).writeBits(1,1);
		}
		if(magnitude == 0){
			uint8_t remainingNeededBits = log2_plus(255 - i);
			uint8_t count = 0;
			for(uint8_t j=0;i + j < 256;j++){
				if(freqs.freqs[i + j]){
					break;
				}
				count++;
			}
			(*sink).writeBits(count - 1,remainingNeededBits);
			i += (count - 1);
		}
		else{
			uint32_t residual = freqs.freqs[i] - (1 << (magnitude - 1));
			if(magnitude - 1 > 8){
				(*sink).writeBits(residual >> 8,magnitude - 9);
				(*sink).writeBits(residual % 256,8);
			}
			else{
				(*sink).writeBits(residual,magnitude - 1);
			}
		}
	}
/*
	for(size_t i=0;i<256;i++){
		printf("table %d %d\n",(int)i,(int)newFreqs.freqs[i]);
	}
*/

	newFreqs.normalize_freqs(1 << 16);
	return newFreqs;
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

uint8_t* encode_grey_8bit_static_ffv1(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	uint8_t* out_buf = new uint8_t[width*height + 1<<20];
	outPointer = out_buf;

	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);

	*(outPointer++) = 0b00011100;//8bit greyscale header

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 0;//ffv1 predictor
	*(outPointer++) = 0b10001000;//use table number 8

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, width, height);

	SymbolStats table = laplace(8);

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		entropy.encodeSymbol(esyms,filtered_bytes[index]);
	}
	delete[] filtered_bytes;

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}

	return out_buf;
}

uint8_t* encode_grey_8bit_ffv1(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	uint8_t* out_buf = new uint8_t[width*height + 1<<20];
	outPointer = out_buf;

	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);

	*(outPointer++) = 0b00011100;//8bit greyscale header

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 0;//ffv1 predictor

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitBuffer tableEncode;
	SymbolStats table = encode_freqTable(stats,&tableEncode);
	tableEncode.conclude();
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		entropy.encodeSymbol(esyms,filtered_bytes[index]);
	}
	delete[] filtered_bytes;

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
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
	/*
	printf("encoding as uncompressed hoh\n");
	uint8_t* out_buf = encode_grey_8bit_simple(grey,width,height,outPointer);
	*/
	/*
	printf("encoding as static predicted\n");
	uint8_t* out_buf = encode_grey_8bit_static_ffv1(grey,width,height,outPointer);
	*/
	printf("encoding as ffv1 predicted\n");
	uint8_t* out_buf = encode_grey_8bit_ffv1(grey,width,height,outPointer);
	
	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[2],out_buf,outPointer - out_buf);
	delete[] out_buf;

	return 0;
}
