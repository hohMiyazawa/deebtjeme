#ifndef SIMPLE_ENCODERS_HEADER
#define SIMPLE_ENCODERS_HEADER

#include "symbolstats.hpp"
#include "bitwriter.hpp"
#include "table_encode.hpp"
#include "rans_byte.h"
#include "entropy_coding.hpp"
#include "filters.hpp"

//working
void encode_raw(uint8_t* in_bytes,size_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	for(size_t index=width*height;index--;){
		*(--outPointer) = in_bytes[index];
	}
	*(--outPointer) = 0b00000000;
}

//working
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
	*(--outPointer) = 0;

	*(--outPointer) = 0b00000010;
}

//working
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

	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b00000000;//one predictor: ffv1
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b00000110;
}

//working
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
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b11010000;
	*(--outPointer) = 0b00010000;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000110;
}

//working
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
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b11010000;
	*(--outPointer) = 0b00000001;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000110;
}


#endif // SIMPLE_ENCODERS_HEADER
