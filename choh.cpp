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
#include "bitwriter.hpp"
#include "table_encode.hpp"
#include "entropy_estimation.hpp"

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

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer);
void encode_left(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer);

void encode_ranged_simple2(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	size_t safety_margin = width*height * (log2_plus(range - 1) + 1) + 2048;

	uint8_t alternates = 1;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing[i] = miniBuffer[i];
	}
	encode_ffv1(in_bytes,range,width,height,miniBuffer[0]);
	//encode_left(in_bytes,range,width,height,miniBuffer[2]);

	uint8_t bestIndex = 0;
	size_t best = miniBuffer[0] - trailing[0];
	for(size_t i=1;i<alternates;i++){
		size_t diff = miniBuffer[i] - trailing[i];
		if(diff < best){
			best = diff;
			bestIndex = i;
		}
	}
	printf("best type: %d\n",(int)bestIndex);
	for(size_t i=0;i<(miniBuffer[bestIndex] - trailing[bestIndex]);i++){
		*(outPointer++) = trailing[bestIndex][i];
	}
	for(size_t i=0;i<alternates;i++){
		delete[] trailing[i];
	}
}

void encode_static_ffv1(uint8_t* in_bytes,size_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00000000;//use no advanced features

	*(outPointer++) = 0b00001000;//use table number 8

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	SymbolStats table = laplace(8,range);

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
	delete[] buffer;
}

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0b00000000;
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
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
	delete[] buffer;
}

void encode_left(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){

	*(outPointer++) = 0b00001001;//use prediction and entropy coding

	*(outPointer++) = 68;//left predictor

	uint8_t* filtered_bytes = filter_all_left(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
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
	delete[] buffer;
}

void entropyCoding_map(
	uint8_t* filtered_bytes,
	uint32_t width,
	uint32_t height,
	SymbolStats* table,
	uint8_t contextNumber,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint8_t*& outPointer
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	RansEncSymbol esyms[contextNumber][256];

	for(size_t context = 0;context < contextNumber;context++){
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
	delete[] buffer;
}

void entropyCoding_map(
	uint8_t* filtered_bytes,
	uint32_t width,
	uint32_t height,
	SymbolStats* table,
	uint8_t* entropyImage,
	uint8_t contextNumber,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint8_t*& outPointer
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	RansEncSymbol esyms[contextNumber][256];

	for(size_t context = 0;context < contextNumber;context++){
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
		entropy.encodeSymbol(esyms[entropyImage[tileIndex]],filtered_bytes[index]);
	}

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
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
	encode_ranged_simple2(
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

	BitWriter tableEncode;
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
		entropyWidth*entropyHeight,
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
	encode_ranged_simple2(
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

	BitWriter tableEncode;
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
	delete[] buffer;
}

void encode_fewPass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	*(outPointer++) = 0b00000110;//use prediction and entropy coding with map

	uint8_t predictorCount = speed*2;
	if(predictorCount > 11){
		predictorCount = 11;
	}
	uint16_t predictorSelection[11] = {
		0,//ffv1
		0b0001000111010000,//avg L-T
		0b0001000111000000,//(1,1,-1,0)
		0b0011000111000001,//(3,1,-1,1)

		0b0001001111000000,//(1,3,-1,0)
		0b0011000011000010,//(3,0,-1,2)
		0b0011000011000001,//(3,0,-1,1)
		0b0010000111000000,//(2,1,-1,0)
		0b0001001011000000,//(1,2,-1,0)
		0b0001000011010000,//(1,0,0,0)
		0b0000000111010000//(0,1,0,0)
	};

	*(outPointer++) = predictorCount - 1;
	for(size_t i=1;i<predictorCount;i++){
		*(outPointer++) = predictorSelection[i] >> 8;
		*(outPointer++) = predictorSelection[i] % 256;
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

	BitWriter tableEncode;
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
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] filtered_bytes[0];
}

static int compare (const void * a, const void * b){
	if (*(double*)a > *(double*)b) return 1;
	else if (*(double*)a < *(double*)b) return -1;
	else return 0;  
}

void encode_optimiser2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	*(outPointer++) = 0b00000110;//use prediction and entropy coding with map

	uint8_t predictorCount = speed*2;
	if(predictorCount > 13){
		predictorCount = 13;
	}
	uint16_t predictorSelection[13] = {
		0,//ffv1
		0b0001000111010000,//avg L-T
		0b0001000111000000,//(1,1,-1,0)
		0b0011000111000001,//(3,1,-1,1)
		0b0001001111000000,//(1,3,-1,0)
		0b0011000011000010,//(3,0,-1,2)
		0b0010000111000001,//(2,1,-1,1)
		0b0011001011000000,//(3,2,-1,0)
		0b0011000011000001,//(3,0,-1,1)
		0b0010000111000000,//(2,1,-1,0)
		0b0001001011000000,//(1,2,-1,0)
		0b0001000011010000,//(1,0,0,0)
		0b0000000111010000//(0,1,0,0)
	};

	*(outPointer++) = predictorCount - 1;
	for(size_t i=1;i<predictorCount;i++){
		*(outPointer++) = predictorSelection[i] >> 8;
		*(outPointer++) = predictorSelection[i] % 256;
	}

	uint32_t predictorWidth_block = 8;
	uint32_t predictorHeight_block = 8;
	uint32_t predictorWidth = (width + predictorWidth_block - 1)/predictorWidth_block;
	uint32_t predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;

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

	printf("starting entropy mapping\n");

	uint32_t entropyWidth_block  = 8;
	uint32_t entropyHeight_block = 8;

	uint32_t entropyWidth  = (width + 7)/8;
	uint32_t entropyHeight = (height + 7)/8;

	uint8_t contextNumber = (width + height + 255)/256;

	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	defaultFreqs.count_freqs(final_bytes, width*height);

	costTable = entropyLookup(defaultFreqs,width*height);

	double entropyMap[entropyWidth*entropyHeight];
	double sortedEntropies[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		double region = regionalEntropy(
			final_bytes,
			costTable,
			i,
			width,
			height,
			entropyWidth_block,
			entropyHeight_block
		);
		entropyMap[i] = region;
		sortedEntropies[i] = region;
	}
	delete[] costTable;

	qsort(sortedEntropies, entropyWidth*entropyHeight, sizeof(double), compare);

	double pivots[contextNumber];
	for(size_t i=0;i<contextNumber;i++){
		pivots[i] = sortedEntropies[(entropyWidth*entropyHeight * (i+1))/contextNumber - 1];
		//printf("pivot: %f\n",pivots[i]);
	}

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i] <= pivots[j]){
				entropyImage[i] = j;
				break;
			}
		}
	}

	SymbolStats contextsUsed;
	contextsUsed.count_freqs(entropyImage, entropyWidth*entropyHeight);

	/*for(size_t i=0;i<contextNumber;i++){
		printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
	}*/
	printf("---\n");

	SymbolStats stats[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}

	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}

	double* costTables[contextNumber];

	for(size_t i=0;i<contextNumber;i++){
		costTables[i] = entropyLookup(stats[i]);
	}

	for(size_t i=0;i<contextNumber;i++){
		contextsUsed.freqs[i] = 0;
	}
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		double regions[contextNumber];
		for(size_t pred=0;pred<contextNumber;pred++){
			regions[pred] = regionalEntropy(
				final_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropyWidth_block,
				entropyHeight_block
			);
		}
		double best = regions[0];
		entropyImage[i] = 0;
		for(size_t pred=1;pred<contextNumber;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropyImage[i] = pred;
			}
		}
		contextsUsed.freqs[entropyImage[i]]++;
	}
	for(size_t i=0;i<contextNumber;i++){
		printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
	}

	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}

	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}
	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
		costTables[i] = entropyLookup(stats[i]);
	}

	//predictor optimisation
	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTables[entropyImage[i]],//assumes predictor block size is equal to entropy block size
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
	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}
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
	//possible loop start?
	for(size_t loop=0;loop < (int)speed - 3;loop++){
		for(size_t context = 0;context < contextNumber;context++){
			for(size_t i=0;i<256;i++){
				stats[context].freqs[i] = 0;
			}
		}

		for(size_t i=0;i<width*height;i++){
			uint8_t cntr = entropyImage[tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			)];
			stats[cntr].freqs[final_bytes[i]]++;
		}

		for(size_t i=0;i<contextNumber;i++){
			costTables[i] = entropyLookup(stats[i]);
		}

		for(size_t i=0;i<contextNumber;i++){
			contextsUsed.freqs[i] = 0;
		}
		for(size_t i=0;i<entropyWidth*entropyHeight;i++){
			double regions[contextNumber];
			for(size_t pred=0;pred<contextNumber;pred++){
				regions[pred] = regionalEntropy(
					final_bytes,
					costTables[pred],
					i,
					width,
					height,
					entropyWidth_block,
					entropyHeight_block
				);
			}
			double best = regions[0];
			entropyImage[i] = 0;
			for(size_t pred=1;pred<contextNumber;pred++){
				if(regions[pred] < best){
					best = regions[pred];
					entropyImage[i] = pred;
				}
			}
			contextsUsed.freqs[entropyImage[i]]++;
		}
		for(size_t i=0;i<contextNumber;i++){
			printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
		}

		for(size_t context = 0;context < contextNumber;context++){
			for(size_t i=0;i<256;i++){
				stats[context].freqs[i] = 0;
			}
		}

		for(size_t i=0;i<width*height;i++){
			uint8_t cntr = entropyImage[tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			)];
			stats[cntr].freqs[final_bytes[i]]++;
		}
		for(size_t i=0;i<contextNumber;i++){
			delete[] costTables[i];
			costTables[i] = entropyLookup(stats[i]);
		}

		//predictor optimisation
		for(size_t i=0;i<predictorWidth*predictorHeight;i++){
			double regions[predictorCount];
			for(size_t pred=0;pred<predictorCount;pred++){
				regions[pred] = regionalEntropy(
					filtered_bytes[pred],
					costTables[entropyImage[i]],//assumes predictor block size is equal to entropy block size
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
		for(size_t i=0;i<contextNumber;i++){
			delete[] costTables[i];
		}
		predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

		for(size_t i=0;i<predictorCount;i++){
			printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
		}
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
	}
	//possible loop end?
	for(size_t i=0;i<predictorCount;i++){
		delete[] filtered_bytes[i];
	}

	uint8_t* trailing = outPointer;
	writeVarint((uint32_t)(predictorWidth - 1), outPointer);
	writeVarint((uint32_t)(predictorHeight - 1),outPointer);
	encode_ranged_simple2(
		predictorImage,
		predictorCount,
		predictorWidth,
		predictorHeight,
		outPointer
	);
	delete[] predictorImage;
	printf("predictor image size: %d bytes\n",(int)(outPointer - trailing));

	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}

	*(outPointer++) = contextNumber - 1;//number of contexts

	trailing = outPointer;
	writeVarint((uint32_t)(entropyWidth - 1), outPointer);
	writeVarint((uint32_t)(entropyHeight - 1),outPointer);
	encode_ranged_simple2(
		entropyImage,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	printf("entropy image size: %d bytes\n",(int)(outPointer - trailing));

	trailing = outPointer;
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(stats[context],tableEncode, range);
	}
	tableEncode.conclude();
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(outPointer - trailing));

	entropyCoding_map(
		final_bytes,
		width,
		height,
		table,
		entropyImage,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropyImage;
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

	printf("creating buffer\n");

	uint8_t* out_buf = new uint8_t[width*height + 4096];
	uint8_t* outPointer = out_buf;

	printf("writing header\n");
	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);

	if(speed == 0){
		encode_static_ffv1(grey, 256,width,height,outPointer);
	}
	else if(speed == 1){
		encode_ffv1(grey, 256,width,height,outPointer);
	}
	else if(speed < 3){
		encode_fewPass(grey, 256,width,height,outPointer, speed);
	}
	else{
		encode_optimiser2(grey, 256,width,height,outPointer, speed);
	}

	
	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[2],out_buf,outPointer - out_buf);
	delete[] out_buf;

	return 0;
}
