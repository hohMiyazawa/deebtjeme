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
#include "2dutils.hpp"

void print_usage(){
	printf("./coder infile.grey width height outfile.hoh speed\n\nspeed is a number from 0-1\n");
}

uint8_t* filter_all_ffv1(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - median3(
					L,
					T,
					L + T - TL
				)
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_median(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - median3(
					in_bytes[y * width + i - 1],
					in_bytes[(y-1) * width + i],
					in_bytes[(y-1) * width + i - 1]
				)
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_left(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[y * width + i - 1];
		}
	}
	return filtered;
}

uint8_t* filter_all_top(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[(y-1) * width + i];
		}
	}
	return filtered;
}

uint8_t* filter_all_generic(uint8_t* in_bytes, uint32_t width, uint32_t height,int a,int b,int c,int d){
	uint8_t* filtered = new uint8_t[width * height];
	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			uint8_t TR = in_bytes[(y-1) * width + i + 1];
			filtered[(y * width) + i] = clamp(
				(int)in_bytes[y * width + i] - (
					a*L + b*T + c*TL + d*TR + halfsum
				)/sum
			);
		}
	}
	return filtered;
}

			/*if(y == 100 && i == 110){
				printf("filtered %d\n",filtered[y * width + i]);
				printf("value %d\n",in_bytes[i]);
				printf("L %d\n",in_bytes[(y) * width + i - 1]);
				printf("T %d\n",in_bytes[(y-1) * width + i]);
				printf("TL %d\n",in_bytes[(y-1) * width + i - 1]);
				printf("median %d\n",median3(
					in_bytes[(y) * width + i - 1],
					in_bytes[(y-1) * width + i],
					in_bytes[(y) * width + i - 1] + in_bytes[(y-1) * width + i] - in_bytes[(y-1) * width + i - 1]
				));
			}*/

double estimateEntropy(uint8_t* in_bytes, size_t size){
	uint8_t frequencies[size];
	for(size_t i=0;i<256;i++){
		frequencies[i] = 0;
	}
	for(size_t i=0;i<size;i++){
		frequencies[in_bytes[i]]++;
	}
	double sum = 0;
	for(size_t i=0;i<256;i++){
		if(frequencies[i]){
			sum += -std::log2((double)frequencies[i]/(double)size) * (double)frequencies[i];
		}
	}
	return sum;
}

const uint32_t prob_bits = 16;
const uint32_t prob_scale = 1 << prob_bits;

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

void binaryPaletteImage(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features

	size_t length = width*height;

	/*uint8_t* filter_bytes = new uint8_t[length];
	filter_bytes[0] = in_bytes[0];

	for(size_t i=1;i<length;i++){
		if(in_bytes[i-1] == in_bytes[i]){
			filter_bytes[i] = 0;
		}
		else{
			filter_bytes[i] = 1;
		}
	}
	in_bytes = filter_bytes;*/

	SymbolStats stats;
	stats.count_freqs(in_bytes, length);
	stats.normalize_freqs(256);
	*(outPointer++) = 8;
	*(outPointer++) = (uint8_t)stats.freqs[0];
	*(outPointer++) = (uint8_t)stats.freqs[1];
	stats.normalize_freqs(1 << 16);

	RansEncSymbol esyms[2];
	RansEncSymbolInit(&esyms[0], stats.cum_freqs[0], stats.freqs[0], 16);
	RansEncSymbolInit(&esyms[1], stats.cum_freqs[1], stats.freqs[1], 16);

	EntropyEncoder entropy;
	for(size_t index=length;index--;){
		entropy.encodeSymbol(esyms,in_bytes[index]);
	}

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	printf("sub image, %d bytes\n",(int)(streamSize + 3));
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}

void writeVarint(uint32_t value,uint8_t*& outPointer){
	if(value < 128){
		*(outPointer++) = (uint8_t)value;
	}
	else if(value < (1<<14)){
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 7);
	}
	else if(value < (1<<21)){
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 7) % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 14);

	}
	else if(value < (1<<28)){
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 7) % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 14) % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 21);	}
	else{
		//nope
	}
}

/*
void simpleCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 0;//use flat entropy table

	for(size_t index=0;index < width*height;index++){
		*(outPointer++) = in_bytes[index];
	}
}


void staticCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 8;//use 8bit entropy table

	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height);
	stats.normalize_freqs(1 << 16);

	RansEncSymbol esyms[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats.freqs[i]) >> 8;
		*(outPointer++) = (stats.freqs[i]) % 256;
		RansEncSymbolInit(&esyms[i], stats.cum_freqs[i], stats.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;--index;){
		entropy.encodeSymbol(esyms,in_bytes[index]);
	}
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}
*/

void ffv1Coder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	//use ffv1 predictor
	*(outPointer++) = 0b10000000;

	*(outPointer++) = 16;//use 16bit entropy table

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, width, height);
	

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	stats.normalize_freqs(1 << 16);

	RansEncSymbol esyms[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats.freqs[i]) >> 8;
		*(outPointer++) = (stats.freqs[i]) % 256;
		RansEncSymbolInit(&esyms[i], stats.cum_freqs[i], stats.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		entropy.encodeSymbol(esyms,filtered_bytes[index]);
	}
	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}

void ffv1Coder_better(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	if(height < 512){
		return ffv1Coder(in_bytes,width,height,out_buf,outPointer);
	}

	*(outPointer++) = 0b11000000;//110: use prediction and enropy image, no LZ | 00000 use awesome predictor
	*(outPointer++) = 0b00000010;//two contexts

	size_t blockSize = 256;
	if(height >= 1536){
		blockSize = 768;
		*(outPointer++) = 0b11110000;
	}
	else if(height >= 1024){
		blockSize = 512;
		*(outPointer++) = 0b11100000;
	}
	else{
		*(outPointer++) = 0b11010000;
	}

	size_t height1 = 1;
	uint32_t b_width = (width + blockSize - 1)/blockSize;
	uint32_t b_height = (height + blockSize - 1)/blockSize;
	size_t total = width*height;

	uint8_t* filtered_bytes  = filter_all_ffv1(in_bytes, width, height);

	uint8_t* entropyIndex = new uint8_t[b_width*b_height];
	for(size_t i=0;i<height1 * b_width;i++){
		entropyIndex[i] = 0;
	}
	for(size_t i=height1 * b_width;i<b_width*b_height;i++){
		entropyIndex[i] = 1;
	}

	binaryPaletteImage(entropyIndex,b_width,b_height,outPointer);

	SymbolStats stats1;
	SymbolStats stats2;
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<(height1 * blockSize) * width;i++){
		stats1.freqs[filtered_bytes[i]]++;
	}
	for(size_t i=(height1 * blockSize) * width;i<total;i++){
		stats2.freqs[filtered_bytes[i]]++;
	}




	*(outPointer++) = 16;//use 16bit entropy table
	stats1.normalize_freqs(1 << 16);

	RansEncSymbol esyms1[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats1.freqs[i]) >> 8;
		*(outPointer++) = (stats1.freqs[i]) % 256;
		RansEncSymbolInit(&esyms1[i], stats1.cum_freqs[i], stats1.freqs[i], 16);
	}

	*(outPointer++) = 16;//use 16bit entropy table
	stats2.normalize_freqs(1 << 16);

	RansEncSymbol esyms2[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats2.freqs[i]) >> 8;
		*(outPointer++) = (stats2.freqs[i]) % 256;
		RansEncSymbolInit(&esyms2[i], stats2.cum_freqs[i], stats2.freqs[i], 16);
	}


	EntropyEncoder entropy;
	for(size_t index=total;index >= (height1 * blockSize) * width;index--){
		entropy.encodeSymbol(esyms2,filtered_bytes[index]);
	}
	for(size_t index=(height1 * blockSize) * width;index--;){
		entropy.encodeSymbol(esyms1,filtered_bytes[index]);
	}

	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] entropyIndex;
	delete[] buffer;
}

double* entropyLookup(SymbolStats stats,size_t total){
	double* table = new double[256];
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i] == 0){
			table[i] = -std::log2(1/(double)total);
		}
		else{
			table[i] = -std::log2(stats.freqs[i]/(double)total);
		}
	}
	return table;
}

double* entropyLookup(SymbolStats stats){
	double* table = new double[256];
	size_t total = 0;
	for(size_t i=0;i<256;i++){
		total += stats.freqs[i];
	}
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i] == 0){
			table[i] = -std::log2(1/(double)total);
		}
		else{
			table[i] = -std::log2(stats.freqs[i]/(double)total);
		}
	}
	return table;
}

double regionalEntropy(
	uint8_t* in_bytes,
	double* entropyTable,
	size_t tileIndex,
	uint32_t width,
	uint32_t height,
	uint32_t b_width,
	uint32_t b_height,
	size_t blockSize
){
	double sum = 0;
	for(size_t y=0;y<blockSize;y++){
		uint32_t y_pos = (tileIndex / b_width)*blockSize + y;
		if(y >= height){
			continue;
		}
		for(size_t x=0;x<blockSize;x++){
			uint32_t x_pos = (tileIndex % b_width)*blockSize + x;
			if(x >= width){
				continue;
			}
			sum += entropyTable[in_bytes[y_pos * width + x_pos]];
		}
	}
	return sum;
}

void fastCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	*(outPointer++) = 0b11000000;//110: use prediction and enropy image, no LZ | 00000 use awesome predictor
	*(outPointer++) = 0b00000010;//two contexts
	*(outPointer++) = 0b00100000;//entropy block size 8

	size_t blockSize = 8;
	uint32_t b_width = (width + blockSize - 1)/blockSize;
	uint32_t b_height = (height + blockSize - 1)/blockSize;

	uint8_t* filtered_bytes  = filter_all_ffv1(in_bytes, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	double* entropyTable = entropyLookup(stats,width*height);

	double entropyMap[b_width*b_height];
	double sum = 0;
	for(size_t i=0;i<b_width*b_height;i++){
		double region = regionalEntropy(
			filtered_bytes,
			entropyTable,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		entropyMap[i] = region;
		sum += region;
	}
	delete[] entropyTable;
	double average = sum/(b_width*b_height);//use for partitioning

	uint8_t* entropyIndex = new uint8_t[b_width*b_height];
	for(size_t i=0;i<b_width*b_height;i++){
		if(entropyMap[i] < average){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}

	binaryPaletteImage(entropyIndex,b_width,b_height,outPointer);

	SymbolStats stats1;
	SymbolStats stats2;
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}




	*(outPointer++) = 16;//use 16bit entropy table
	stats1.normalize_freqs(1 << 16);

	RansEncSymbol esyms1[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats1.freqs[i]) >> 8;
		*(outPointer++) = (stats1.freqs[i]) % 256;
		RansEncSymbolInit(&esyms1[i], stats1.cum_freqs[i], stats1.freqs[i], 16);
	}

	*(outPointer++) = 16;//use 16bit entropy table
	stats2.normalize_freqs(1 << 16);

	RansEncSymbol esyms2[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats2.freqs[i]) >> 8;
		*(outPointer++) = (stats2.freqs[i]) % 256;
		RansEncSymbolInit(&esyms2[i], stats2.cum_freqs[i], stats2.freqs[i], 16);
	}


	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		if(entropyIndex[tileIndexFromPixel(
			index,
			width,
			b_width,
			blockSize
		)] == 0){
			entropy.encodeSymbol(esyms1,filtered_bytes[index]);
		}
		else{
			entropy.encodeSymbol(esyms2,filtered_bytes[index]);
		}
	}
	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] entropyIndex;
	delete[] buffer;
}

void fastCoder2(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	*(outPointer++) = 0b11000000;//110: use prediction and enropy image, no LZ | 00000 use awesome predictor
	*(outPointer++) = 0b00000010;//two contexts
	*(outPointer++) = 0b00100000;//entropy block size 8

	size_t blockSize = 8;
	uint32_t b_width = (width + blockSize - 1)/blockSize;
	uint32_t b_height = (height + blockSize - 1)/blockSize;

	uint8_t* filtered_bytes  = filter_all_ffv1(in_bytes, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	double* entropyTable = entropyLookup(stats,width*height);

	double entropyMap[b_width*b_height];
	double sum = 0;
	for(size_t i=0;i<b_width*b_height;i++){
		double region = regionalEntropy(
			filtered_bytes,
			entropyTable,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		entropyMap[i] = region;
		sum += region;
	}
	delete[] entropyTable;
	double average = sum/(b_width*b_height);//use for partitioning

	uint8_t* entropyIndex = new uint8_t[b_width*b_height];
	for(size_t i=0;i<b_width*b_height;i++){
		if(entropyMap[i] < average){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}

	SymbolStats stats1;
	SymbolStats stats2;
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}

	double* entropyTable1 = entropyLookup(stats1);
	double* entropyTable2 = entropyLookup(stats2);

	for(size_t i=0;i<b_width*b_height;i++){
		double region1 = regionalEntropy(
			filtered_bytes,
			entropyTable1,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		double region2 = regionalEntropy(
			filtered_bytes,
			entropyTable2,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		if(region1 < region2){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}


	delete[] entropyTable1;
	delete[] entropyTable2;

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}

	binaryPaletteImage(entropyIndex,b_width,b_height,outPointer);

	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}




	*(outPointer++) = 16;//use 16bit entropy table
	stats1.normalize_freqs(1 << 16);

	RansEncSymbol esyms1[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats1.freqs[i]) >> 8;
		*(outPointer++) = (stats1.freqs[i]) % 256;
		RansEncSymbolInit(&esyms1[i], stats1.cum_freqs[i], stats1.freqs[i], 16);
	}

	*(outPointer++) = 16;//use 16bit entropy table
	stats2.normalize_freqs(1 << 16);

	RansEncSymbol esyms2[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats2.freqs[i]) >> 8;
		*(outPointer++) = (stats2.freqs[i]) % 256;
		RansEncSymbolInit(&esyms2[i], stats2.cum_freqs[i], stats2.freqs[i], 16);
	}


	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		if(entropyIndex[tileIndexFromPixel(
			index,
			width,
			b_width,
			blockSize
		)] == 0){
			entropy.encodeSymbol(esyms1,filtered_bytes[index]);
		}
		else{
			entropy.encodeSymbol(esyms2,filtered_bytes[index]);
		}
	}
	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] entropyIndex;
	delete[] buffer;
}


void slowCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	*(outPointer++) = 0b11000010;//110: use prediction and enropy image, no LZ | 00010 use two predictor

	//predictor indices are TBD
	*(outPointer++) = 0b00000110;//ffv1
	*(outPointer++) = 0b00000111;//median
/*	*(outPointer++) = 0b01000100;//left
	*(outPointer++) = 0b00010100;//top
	*(outPointer++) = 0b01010100;//L-T
	*(outPointer++) = 0b11000010;//[3,0,-1,2]
	*(outPointer++) = 0b01100000;//[1,2,-1,0]
	*(outPointer++) = 0b01010000;//[1,1,-1,0]
*/

	*(outPointer++) = 0b00100000;//four contexts, entropy block size 8
	*(outPointer++) = 0b01000010;

	size_t blockSize = 8;
	uint32_t b_width = (width + blockSize - 1)/blockSize;
	uint32_t b_height = (height + blockSize - 1)/blockSize;

	uint8_t* filtered_bytes  = filter_all_ffv1(in_bytes, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	double* entropyTable = entropyLookup(stats,width*height);

	double entropyMap[b_width*b_height];
	double sum = 0;
	for(size_t i=0;i<b_width*b_height;i++){
		double region = regionalEntropy(
			filtered_bytes,
			entropyTable,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		entropyMap[i] = region;
		sum += region;
	}
	delete[] entropyTable;
	double average = sum/(b_width*b_height);//use for partitioning

	uint8_t* entropyIndex = new uint8_t[b_width*b_height];
	for(size_t i=0;i<b_width*b_height;i++){
		if(entropyMap[i] < average){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}

	SymbolStats stats1;
	SymbolStats stats2;
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}

	double* entropyTable1 = entropyLookup(stats1);
	double* entropyTable2 = entropyLookup(stats2);

	for(size_t i=0;i<b_width*b_height;i++){
		double region1 = regionalEntropy(
			filtered_bytes,
			entropyTable1,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		double region2 = regionalEntropy(
			filtered_bytes,
			entropyTable2,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		if(region1 < region2){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}


	delete[] entropyTable1;
	delete[] entropyTable2;

	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}

	entropyTable1 = entropyLookup(stats1);
	entropyTable2 = entropyLookup(stats2);

	uint8_t* filtered_bytes_median  = filter_all_median(in_bytes, width, height);

	uint8_t* predictorIndex = new uint8_t[b_width*b_height];

	for(size_t i=0;i<b_width*b_height;i++){
		if(entropyIndex[i] == 0){
			double region1 = regionalEntropy(
				filtered_bytes,
				entropyTable1,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			double region2 = regionalEntropy(
				filtered_bytes_median,
				entropyTable1,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			if(region1 < region2){
				predictorIndex[i] = 0;
			}
			else{
				predictorIndex[i] = 1;
			}
		}
		else{
			double region1 = regionalEntropy(
				filtered_bytes,
				entropyTable2,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			double region2 = regionalEntropy(
				filtered_bytes_median,
				entropyTable2,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			if(region1 < region2){
				predictorIndex[i] = 0;
			}
			else{
				predictorIndex[i] = 1;
			}
		}
	}

	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		size_t place = tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		);
		if(predictorIndex[place] == 0){
			if(entropyIndex[place] == 0){
				stats1.freqs[filtered_bytes[i]]++;
			}
			else{
				stats2.freqs[filtered_bytes[i]]++;
			}
		}
		else{
			if(entropyIndex[place] == 0){
				stats1.freqs[filtered_bytes_median[i]]++;
			}
			else{
				stats2.freqs[filtered_bytes_median[i]]++;
			}
		}
	}

	delete[] entropyTable1;
	delete[] entropyTable2;


	entropyTable1 = entropyLookup(stats1);
	entropyTable2 = entropyLookup(stats2);

	for(size_t i=0;i<b_width*b_height;i++){
		if(predictorIndex[i] == 0){
			double region1 = regionalEntropy(
				filtered_bytes,
				entropyTable1,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			double region2 = regionalEntropy(
				filtered_bytes,
				entropyTable2,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			if(region1 < region2){
				entropyIndex[i] = 0;
			}
			else{
				entropyIndex[i] = 1;
			}
		}
		else{
			double region1 = regionalEntropy(
				filtered_bytes_median,
				entropyTable1,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			double region2 = regionalEntropy(
				filtered_bytes_median,
				entropyTable2,
				i,
				width,
				height,
				b_width,
				b_height,
				blockSize
			);
			if(region1 < region2){
				entropyIndex[i] = 0;
			}
			else{
				entropyIndex[i] = 1;
			}
		}
	}




/////
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		size_t place = tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		);
		if(predictorIndex[place] == 0){
			if(entropyIndex[place] == 0){
				stats1.freqs[filtered_bytes[i]]++;
			}
			else{
				stats2.freqs[filtered_bytes[i]]++;
			}
		}
		else{
			if(entropyIndex[place] == 0){
				stats1.freqs[filtered_bytes_median[i]]++;
			}
			else{
				stats2.freqs[filtered_bytes_median[i]]++;
			}
		}
	}

	binaryPaletteImage(predictorIndex,b_width,b_height,outPointer);
	binaryPaletteImage(entropyIndex,b_width,b_height,outPointer);

	*(outPointer++) = 16;//use 16bit entropy table
	stats1.normalize_freqs(1 << 16);

	RansEncSymbol esyms1[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats1.freqs[i]) >> 8;
		*(outPointer++) = (stats1.freqs[i]) % 256;
		RansEncSymbolInit(&esyms1[i], stats1.cum_freqs[i], stats1.freqs[i], 16);
	}

	*(outPointer++) = 16;//use 16bit entropy table
	stats2.normalize_freqs(1 << 16);

	RansEncSymbol esyms2[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats2.freqs[i]) >> 8;
		*(outPointer++) = (stats2.freqs[i]) % 256;
		RansEncSymbolInit(&esyms2[i], stats2.cum_freqs[i], stats2.freqs[i], 16);
	}


	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		size_t place = tileIndexFromPixel(
			index,
			width,
			b_width,
			blockSize
		);
		if(predictorIndex[place] == 0){
			if(entropyIndex[place] == 0){
				entropy.encodeSymbol(esyms1,filtered_bytes[index]);
			}
			else{
				entropy.encodeSymbol(esyms2,filtered_bytes[index]);
			}
		}
		else{
			if(entropyIndex[place] == 0){
				entropy.encodeSymbol(esyms1,filtered_bytes_median[index]);
			}
			else{
				entropy.encodeSymbol(esyms2,filtered_bytes_median[index]);
			}
		}
	}
	delete[] filtered_bytes;
	delete[] filtered_bytes_median;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] entropyIndex;
	delete[] predictorIndex;
	delete[] buffer;
	delete[] entropyTable1;
	delete[] entropyTable2;
}

int main(int argc, char *argv[]){
	if(argc < 6){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	uint32_t width = atoi(argv[2]);
	uint32_t height = atoi(argv[3]);

	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	if(
		width == 0 || height == 0
		|| (width*height) != in_size
	){
		printf("invalid width or height\n");
		print_usage();
		return 2;
	}

	printf("read %d bytes\n",(int)in_size);
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	size_t cruncher_mode = 0;
	if(argc > 4 && strcmp(argv[5],"-1") == 0){
		cruncher_mode = -1;
	}
	else if(argc > 4 && strcmp(argv[5],"0") == 0){
		cruncher_mode = 0;
	}
	else if(argc > 4 && strcmp(argv[5],"1") == 0){
		cruncher_mode = 1;
	}
	else if(argc > 4 && strcmp(argv[5],"2") == 0){
		cruncher_mode = 2;
	}
	else if(argc > 4 && strcmp(argv[5],"3") == 0){
		cruncher_mode = 3;
	}
	else if(argc > 4){
		printf("invalid speed setting\n");
		print_usage();
	}

	uint8_t* out_buf = new uint8_t[32<<20];
	uint8_t* outPointer = out_buf;
	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);
	//simpleCoder(in_bytes,width,height,out_buf,outPointer);
	//staticCoder(in_bytes,width,height,out_buf,outPointer);
	if(cruncher_mode == 0){
		ffv1Coder_better(in_bytes,width,height,out_buf,outPointer);
	}
	else if(cruncher_mode == 1){
		fastCoder(in_bytes,width,height,out_buf,outPointer);
	}
	else if(cruncher_mode == 2){
		fastCoder2(in_bytes,width,height,out_buf,outPointer);
	}
	else if(cruncher_mode == 3){
		slowCoder(in_bytes,width,height,out_buf,outPointer);
	}
	delete[] in_bytes;

	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[4],out_buf,outPointer - out_buf);
	delete[] out_buf;
	return 0;
}
