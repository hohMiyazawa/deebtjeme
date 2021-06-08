#ifndef ENTROPY_CODING_HEADER
#define ENTROPY_CODING_HEADER

#include "rans_byte.h"
#include "symbolstats.hpp"

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

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		size_t tileIndex = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		RansEncPutSymbol(&rans, &outPointer, esyms[tileIndex] + filtered_bytes[index]);//index == context here, but that's not generally true. Use a map lookup!
	}
	RansEncFlush(&rans, &outPointer);
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

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		size_t tileIndex = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropyImage[tileIndex]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
}

#endif //ENTROPY_CODING
