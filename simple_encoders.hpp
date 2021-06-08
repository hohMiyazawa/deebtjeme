#ifndef SIMPLE_ENCODERS_HEADER
#define SIMPLE_ENCODERS_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_coding.hpp"

void encode_entropy(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0b00000000;

	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height);

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
		entropy.encodeSymbol(esyms,in_bytes[index]);
	}

	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0b00000100;
	*(outPointer++) = 0;
	*(outPointer++) = 0;
	*(outPointer++) = 0;
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
	*(outPointer++) = 0b00000100;
	*(outPointer++) = 0;
	*(outPointer++) = 0b00010000;//left predictor upper
	*(outPointer++) = 0b11010000;//left predictor lower

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

void encode_top(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0b00000100;
	*(outPointer++) = 0;
	*(outPointer++) = 0b00000001;//top predictor upper
	*(outPointer++) = 0b11010000;//top predictor lower

	uint8_t* filtered_bytes = filter_all_top(in_bytes, range, width, height);

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


#endif // SIMPLE_ENCODERS_HEADER
