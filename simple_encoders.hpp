#ifndef SIMPLE_ENCODERS_HEADER
#define SIMPLE_ENCODERS_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_coding.hpp"

void encode_entropy(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + in_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	*(--outPointer) = 0b00000000;
}

void encode_ffv1(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	*(--outPointer) = 0;
	*(--outPointer) = 0;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000100;
}

void encode_left(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	uint8_t* filtered_bytes = filter_all_left(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	*(--outPointer) = 0b11010000;
	*(--outPointer) = 0b00010000;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000100;
}

void encode_top(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	uint8_t* filtered_bytes = filter_all_top(in_bytes, range, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	*(--outPointer) = 0b11010000;
	*(--outPointer) = 0b00000001;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000100;
}

void encode_quad(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){

	if(width*height < 16){//no sense in doing this for small images
		return encode_entropy(in_bytes,range,width,height,outPointer);
	}

	uint8_t contextNumber = 4;

	uint8_t* entropy_image = new uint8_t[contextNumber];
	for(size_t i=0;i<contextNumber;i++){
		entropy_image[i] = i;
	}

	uint32_t entropyWidth = 2;
	uint32_t entropyHeight = 2;


	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats* statistics = new SymbolStats[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		SymbolStats stats;
		statistics[context] = stats;
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropy_image[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		statistics[cntr].freqs[in_bytes[i]]++;
	}
//encode data
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context],tableEncode, range);
	}
	delete[] statistics;
	tableEncode.conclude();

	entropyCoding_map(
		in_bytes,
		width,
		height,
		table,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	encode_entropy(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);

	*(--outPointer) = contextNumber - 1;
	*(--outPointer) = 0b00000010;
}


#endif // SIMPLE_ENCODERS_HEADER
