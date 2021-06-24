#ifndef INDEX_ENCODER_HEADER
#define INDEX_ENCODER_HEADER

#include "simple_encoders.hpp"
#include "colour_simple_encoders.hpp"

void indexed_encode_speed0(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer
){
	uint8_t in_bytes[width*height];
	for(size_t i=0;i<width*height;i++){
		for(size_t j=0;j<colour_count;j++){
			if(
				palette[j*3] == raw_bytes[i*3]
				&& palette[j*3+1] == raw_bytes[i*3+1]
				&& palette[j*3+2] == raw_bytes[i*3+2]
			){
				in_bytes[i] = j;
				break;
			}
		}
	}
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

	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;

	*(--outPointer) = 0b00110010;
}

void indexed_encode_speed1(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer
){
	uint8_t in_bytes[width*height];
	for(size_t i=0;i<width*height;i++){
		for(size_t j=0;j<colour_count;j++){
			if(
				palette[j*3] == raw_bytes[i*3]
				&& palette[j*3+1] == raw_bytes[i*3+1]
				&& palette[j*3+2] == raw_bytes[i*3+2]
			){
				in_bytes[i] = j;
				break;
			}
		}
	}
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, colour_count, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode, colour_count);
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

	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;

	*(--outPointer) = 0b00110110;
}

void indexed_encode(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	if(speed == 0){
		indexed_encode_speed0(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer
		);
	}
	else{
		indexed_encode_speed1(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer
		);
	}
}

#endif //INDEX_ENCODER_HEADER
