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
		uint8_t buffer[65536];
		size_t length;
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
	if(partial_length){
		buffer[length++] = partial;
	}
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

SymbolStats encode_freqTable(SymbolStats freqs,BitBuffer& sink, uint32_t range){
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

	//printf("first in buffer %d %d\n",(int)sink.buffer[0],(int)sink.length);
	sink.writeBits(0,1);
	sink.writeBits(3,2);

	size_t sum = 0;
	for(size_t i=0;i<range;i++){//technically, there should be no frequencies above the range
		sum += freqs.freqs[i];
	}
	if(sum > (1 << 16)){
		freqs.normalize_freqs(1 << 16);
	}
	SymbolStats newFreqs;
	for(size_t i=0;i<256;i++){
		newFreqs.freqs[i] = freqs.freqs[i];
	}
	for(size_t i=0;i<range;i++){
		uint8_t magnitude = log2_plus(freqs.freqs[i]);
		if(magnitude < 15){
			sink.writeBits(magnitude,4);
		}
		else if(magnitude == 15){
			sink.writeBits(15,4);
			sink.writeBits(0,1);
		}
		else if(magnitude == 16){
			sink.writeBits(15,4);
			sink.writeBits(1,1);
		}
		if(magnitude == 0){
			uint8_t remainingNeededBits = log2_plus(range - 1 - i);
			uint8_t count = 0;
			for(uint8_t j=0;i + j < range;j++){
				if(freqs.freqs[i + j]){
					break;
				}
				count++;
			}
			sink.writeBits(count - 1,remainingNeededBits);
			i += (count - 1);
		}
		else{
			uint32_t residual = freqs.freqs[i] - (1 << (magnitude - 1));
			if(magnitude - 1 > 8){
				sink.writeBits(residual >> 8,magnitude - 9);
				sink.writeBits(residual % 256,8);
			}
			else{
				sink.writeBits(residual,magnitude - 1);
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

void encode_ranged_simple(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0b00000000;//use no compression features
	if(range == 256){
		for(size_t i=0;i<width*height;i++){
			*(outPointer++) = in_bytes[i];
		}
	}
	else{
		BitBuffer sink;
		uint8_t bitDepth = log2_plus(range - 1);
		for(size_t i=0;i<width*height;i++){
			sink.writeBits(in_bytes[i],bitDepth);
		}
		sink.conclude();
		for(size_t i=0;i<sink.length;i++){
			*(outPointer++) = sink.buffer[i];
		}
	}
}

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer);
void encode_left(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer);

void encode_ranged_simple2(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	if(width*height <= 16){//simple coding for small ones
		encode_ranged_simple(in_bytes,range,width,height,outPointer);
		return;
	}
	//encode_ranged_simple(in_bytes,range,width,height,outPointer);
	encode_left(in_bytes,range,width,height,outPointer);
}

void encode_grey_8bit_static_ffv1(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 0;//ffv1 predictor
	*(outPointer++) = 0b10001000;//use table number 8

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, 256, width, height);

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
}

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 0;//ffv1 predictor

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitBuffer tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
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
}

void encode_left(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 68;//left predictor

	uint8_t* filtered_bytes = filter_all_left(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitBuffer tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
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
}

void entropyCoding_map(
	uint8_t* filtered_bytes,
	uint32_t width,
	uint32_t height,
	SymbolStats* table,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint8_t*& outPointer
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	RansEncSymbol esyms[entropyWidth*entropyHeight][256];

	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms[context][i], table[context].cum_freqs[i], table[context].freqs[i], 16);
		}
	}
	printf("starting entropy coding\n");

	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		size_t tileIndex = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		entropy.encodeSymbol(esyms[tileIndex],filtered_bytes[index]);//index == context here, but that's not generally true. Use a map lookup!
	}

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
}

void encode_grey_8bit_entropyMap_ffv1(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	uint32_t entropyWidth  = (width)/128;
	uint32_t entropyHeight = (height)/128;
	if(entropyWidth == 0){
		entropyWidth = 1;
	}
	if(entropyHeight == 0){
		entropyHeight = 1;
	}
	if(entropyWidth * entropyHeight == 1){
		encode_ffv1(in_bytes, 256,width,height,outPointer);
		return;
	}
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	*(outPointer++) = 0b00001101;//use prediction and entropy coding with map

	*(outPointer++) = 0;//ffv1 predictor

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, 256, width, height);

	SymbolStats stats[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		stats[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)].freqs[filtered_bytes[i]]++;
	}

	*(outPointer++) = entropyWidth*entropyHeight - 1;//number of contexts

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		entropyImage[i] = i;//all contexts are unique
	}

	writeVarint((uint32_t)(entropyWidth - 1), outPointer);
	writeVarint((uint32_t)(entropyHeight - 1),outPointer);
	encode_ranged_simple(
		entropyImage,
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropyImage;//don't need it, since all the contexts are unique
	/*printf("  cbuffer content %d\n",(int)(*(outPointer-3)));
	printf("  cbuffer content %d\n",(int)(*(outPointer-2)));
	printf("  cbuffer content %d\n",(int)(*(outPointer-1)));*/

	BitBuffer tableEncode;
	SymbolStats table[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		table[context] = encode_freqTable(stats[context],tableEncode,256);
	}
	tableEncode.conclude();
	//printf("  buffer content %d\n",(int)tableEncode.buffer[tableEncode.length - 2]);
	//printf("  buffer content %d\n",(int)tableEncode.buffer[tableEncode.length - 1]);
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}

	entropyCoding_map(
		filtered_bytes,
		width,
		height,
		table,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] filtered_bytes;
}

void encode_grey_predictorMap(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 253;//use two predictors
	*(outPointer++) = 0;//ffv1
	*(outPointer++) = 68;//left

	uint32_t predictorWidth = 2;
	uint32_t predictorHeight = 1;
	uint32_t predictorWidth_block = (width + 1)/predictorWidth;
	uint32_t predictorHeight_block = height;
	uint8_t* predictorImage = new uint8_t[predictorWidth*predictorHeight];
	predictorImage[0] = 0;
	predictorImage[1] = 1;

	writeVarint((uint32_t)(predictorWidth - 1), outPointer);
	writeVarint((uint32_t)(predictorHeight - 1),outPointer);
	encode_ranged_simple(
		predictorImage,
		predictorWidth*predictorHeight,
		predictorWidth,
		predictorHeight,
		outPointer
	);
	delete[] predictorImage;//don't need it, since all the contexts are unique

	uint8_t* filtered_bytes1 = filter_all_ffv1(in_bytes, 256, width, height);
	uint8_t* filtered_bytes2 = filter_all_left(in_bytes, 256, width, height);

	SymbolStats stats;
	for(size_t i=0;i<256;i++){
		stats.freqs[i] = 0;
	}
	for(size_t i=0;i<width*height;i++){
		if((i % width)/ predictorWidth_block){
			stats.freqs[filtered_bytes2[i]]++;
		}
		else{
			stats.freqs[filtered_bytes1[i]]++;
		}
	}

	BitBuffer tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,256);
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
		if((index % width)/ predictorWidth_block){
			entropy.encodeSymbol(esyms,filtered_bytes2[index]);
		}
		else{
			entropy.encodeSymbol(esyms,filtered_bytes1[index]);
		}
	}
	delete[] filtered_bytes1;
	delete[] filtered_bytes2;

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
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
	uint32_t b_width_block,
	uint32_t b_height_block
){
	uint32_t b_width = (width + b_width_block - 1)/b_width_block;
	double sum = 0;
	for(size_t y=0;y<b_height_block;y++){
		uint32_t y_pos = (tileIndex / b_width)*b_height_block + y;
		if(y >= height){
			continue;
		}
		for(size_t x=0;x<b_width_block;x++){
			uint32_t x_pos = (tileIndex % b_width)*b_width_block + x;
			if(x >= width){
				continue;
			}
			sum += entropyTable[in_bytes[y_pos * width + x_pos]];
		}
	}
	return sum;
}

void encode_fewPass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	*(outPointer++) = 0b00001101;//use prediction and entropy coding with map

	uint8_t predictorCount = speed*2;
	if(predictorCount > 12){
		predictorCount = 12;
	}
	uint8_t predictorSelection[12] = {
		0,//ffv1
		0b01010100,//avg L-T
		0b01010000,//(1,1,-1,0)
		0b11010001,//(3,1,-1,1)

		0b01110000,//(1,3,-1,0)
		0b11000010,//(3,0,-1,2)
		0b11000001,//(3,0,-1,1)
		0b10010000,//(2,1,-1,0)
		0b01100000,//(1,2,-1,0)
		0b01000100,//(1,0,0,0)
		6,//median
		0b00010100//(0,1,0,0)
	};

	*(outPointer++) = 255 - predictorCount;//use three predictors
	for(size_t i=0;i<predictorCount;i++){
		*(outPointer++) = predictorSelection[i];
	}

	uint32_t predictorWidth_block = 8;
	uint32_t predictorHeight_block = 8;
	uint32_t predictorWidth = (width + predictorWidth_block - 1)/predictorWidth_block;
	uint32_t predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;
	writeVarint((uint32_t)(predictorWidth - 1), outPointer);
	writeVarint((uint32_t)(predictorHeight - 1),outPointer);

	uint8_t *filtered_bytes[predictorCount];

	for(size_t i=0;i<predictorCount;i++){
		filtered_bytes[i] = filter_all(in_bytes, range, width, height, predictorSelection[i]);
	}

	SymbolStats defaultFreqs;
	defaultFreqs.count_freqs(filtered_bytes[0], width*height);

	double* costTable = entropyLookup(defaultFreqs,width*height);

	uint8_t* predictorImage = new uint8_t[predictorWidth*predictorHeight];

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTable,
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	SymbolStats predictorsUsed;
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}

	delete[] costTable;

	encode_ranged_simple2(
		predictorImage,
		predictorCount,
		predictorWidth,
		predictorHeight,
		outPointer
	);

	printf("merging filtered bitplanes\n");
	for(size_t i=0;i<width*height;i++){
		filtered_bytes[0][i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}
	for(size_t i=1;i<predictorCount;i++){
		delete[] filtered_bytes[i];
	}
	delete[] predictorImage;
	printf("starting entropy mapping\n");

	uint32_t entropyWidth  = (width)/128;
	uint32_t entropyHeight = (height)/128;
	if(entropyWidth == 0){
		entropyWidth = 1;
	}
	if(entropyHeight == 0){
		entropyHeight = 1;
	}
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	SymbolStats stats[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		stats[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)].freqs[filtered_bytes[0][i]]++;
	}

	*(outPointer++) = entropyWidth*entropyHeight - 1;//number of contexts

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		entropyImage[i] = i;//all contexts are unique
	}

	writeVarint((uint32_t)(entropyWidth - 1), outPointer);
	writeVarint((uint32_t)(entropyHeight - 1),outPointer);
	encode_ranged_simple2(
		entropyImage,
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropyImage;

	BitBuffer tableEncode;
	SymbolStats table[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		table[context] = encode_freqTable(stats[context],tableEncode, range);
	}
	tableEncode.conclude();
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}

	entropyCoding_map(
		filtered_bytes[0],
		width,
		height,
		table,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] filtered_bytes[0];
}

void encode_fewPass2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	*(outPointer++) = 0b00001101;//use prediction and entropy coding with map

	uint8_t predictorCount = speed*2;
	if(predictorCount > 12){
		predictorCount = 12;
	}
	uint8_t predictorSelection[12] = {
		0,//ffv1
		0b01010100,//avg L-T
		0b01010000,//(1,1,-1,0)
		0b11010001,//(3,1,-1,1)

		0b01110000,//(1,3,-1,0)
		0b11000010,//(3,0,-1,2)
		0b11000001,//(3,0,-1,1)
		0b10010000,//(2,1,-1,0)
		0b01100000,//(1,2,-1,0)
		0b01000100,//(1,0,0,0)
		6,//median
		0b00010100//(0,1,0,0)
	};

	*(outPointer++) = 255 - predictorCount;//use three predictors
	for(size_t i=0;i<predictorCount;i++){
		*(outPointer++) = predictorSelection[i];
	}

	uint32_t predictorWidth_block = 8;
	uint32_t predictorHeight_block = 8;
	uint32_t predictorWidth = (width + predictorWidth_block - 1)/predictorWidth_block;
	uint32_t predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;
	writeVarint((uint32_t)(predictorWidth - 1), outPointer);
	writeVarint((uint32_t)(predictorHeight - 1),outPointer);

	uint8_t *filtered_bytes[predictorCount];

	for(size_t i=0;i<predictorCount;i++){
		filtered_bytes[i] = filter_all(in_bytes, range, width, height, predictorSelection[i]);
	}

	SymbolStats defaultFreqs;
	defaultFreqs.count_freqs(filtered_bytes[0], width*height);

	double* costTable = entropyLookup(defaultFreqs,width*height);

	uint8_t* predictorImage = new uint8_t[predictorWidth*predictorHeight];

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTable,
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	SymbolStats predictorsUsed;
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}

	delete[] costTable;

	printf("merging filtered bitplanes\n");
	uint8_t* final_bytes = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		final_bytes[i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}

	defaultFreqs.count_freqs(final_bytes, width*height);

	costTable = entropyLookup(defaultFreqs,width*height);

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTable,
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}

	delete[] costTable;

	encode_ranged_simple2(
		predictorImage,
		predictorCount,
		predictorWidth,
		predictorHeight,
		outPointer
	);




	printf("merging filtered bitplanes again\n");
	for(size_t i=0;i<width*height;i++){
		final_bytes[i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}
	for(size_t i=0;i<predictorCount;i++){
		delete[] filtered_bytes[i];
	}
	delete[] predictorImage;
	printf("starting entropy mapping\n");

	uint32_t entropyWidth  = (width)/128;
	uint32_t entropyHeight = (height)/128;
	if(entropyWidth == 0){
		entropyWidth = 1;
	}
	if(entropyHeight == 0){
		entropyHeight = 1;
	}
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	SymbolStats stats[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		stats[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)].freqs[final_bytes[i]]++;
	}

	*(outPointer++) = entropyWidth*entropyHeight - 1;//number of contexts

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		entropyImage[i] = i;//all contexts are unique
	}

	writeVarint((uint32_t)(entropyWidth - 1), outPointer);
	writeVarint((uint32_t)(entropyHeight - 1),outPointer);
	encode_ranged_simple2(
		entropyImage,
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropyImage;

	BitBuffer tableEncode;
	SymbolStats table[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		table[context] = encode_freqTable(stats[context],tableEncode, range);
	}
	tableEncode.conclude();
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}

	entropyCoding_map(
		final_bytes,
		width,
		height,
		table,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] final_bytes;
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

	uint8_t* out_buf = new uint8_t[width*height + 1<<20];
	uint8_t* outPointer = out_buf;

	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);

	*(outPointer++) = 0b00011100;//8bit greyscale header

	/*
	printf("encoding as uncompressed hoh\n");
	encode_ranged_simple(grey,256,width,height,outPointer);
	*/
	/*
	printf("encoding as static predicted\n");
	encode_grey_8bit_static_ffv1(grey,width,height,outPointer);
	*/
	/*
	printf("encoding as ffv1 predicted\n");
	encode_ffv1(grey,width,height,outPointer);
	*/
/*
	printf("encoding as ffv1 predicted and entropy mapped\n");
	encode_grey_8bit_entropyMap_ffv1(grey,width,height,outPointer);
*/
	/*
	printf("encoding as left-right\n");
	encode_grey_predictorMap(grey,width,height,outPointer);
	*/
/*
	printf("encoding as fewpass\n");
	encode_fewPass(grey, 256,width,height,outPointer);
*/
	if(speed == 0){
		encode_grey_8bit_entropyMap_ffv1(grey,width,height,outPointer);
	}
	else if(speed < 4){
		encode_fewPass(grey, 256,width,height,outPointer, speed);
	}
	else{
		encode_fewPass2(grey, 256,width,height,outPointer, speed);
	}

	
	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[2],out_buf,outPointer - out_buf);
	delete[] out_buf;

	return 0;
}
