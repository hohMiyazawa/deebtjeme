#ifndef OPTIMISER_HEADER
#define OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "entropy_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"

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

void research_optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	*(outPointer++) = 0b00000010;//use entropy coding with a map

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);

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
		statistics[cntr].freqs[filtered_bytes[i]]++;
	}

	for(size_t i=0;i<speed;i++){
		contextNumber = entropy_redistribution_pass(
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics
		);
	}
///encode data

	*(outPointer++) = contextNumber - 1;//number of contexts

	uint8_t* trailing = outPointer;
	writeVarint((uint32_t)(entropyWidth - 1), outPointer);
	writeVarint((uint32_t)(entropyHeight - 1),outPointer);
	encode_ranged_simple2(
		entropy_image,
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
		table[context] = encode_freqTable(statistics[context],tableEncode, range);
	}
	tableEncode.conclude();
	for(size_t i=0;i<tableEncode.length;i++){
		*(outPointer++) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(outPointer - trailing));

	entropyCoding_map(
		filtered_bytes,
		width,
		height,
		table,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] statistics;
	delete[] filtered_bytes;
	delete[] entropy_image;
}

#endif //OPTIMISER
