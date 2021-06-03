#ifndef ENTROPY_CODING_HEADER
#define ENTROPY_CODING_HEADER

#include "rans_byte.h"
#include "symbolstats.hpp"

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

#endif //ENTROPY_CODING
