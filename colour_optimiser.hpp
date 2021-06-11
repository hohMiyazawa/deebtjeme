#ifndef OPTIMISER_HEADER
#define OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "colour_entropy_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "colour_simple_encoders.hpp"
#include "colour_predictor_optimiser.hpp"

void colour_optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colourSub_filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = colour_entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);
	printf("contexts: %d\n",(int)contextNumber);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats statistics[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		size_t tile_index = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		statistics[entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
		statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
		statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
	for(size_t i=0;i<speed;i++){
		contextNumber = colour_entropy_redistribution_pass(
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
	printf("table started\n");
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, range);
	}
	tableEncode.conclude();


	RansEncSymbol esyms[contextNumber][256];

	for(size_t cont=0;cont<contextNumber;cont++){
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms[cont][i], table[cont].cum_freqs[i], table[cont].freqs[i], 16);
		}
	}

	printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		size_t tile_index = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 2]] + filtered_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 1]] + filtered_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 0]] + filtered_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	colour_encode_entropy_channel(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	*(--outPointer) = 0b00000000;//one predictor: ffv1
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b10000110;
}

void colour_optimiser_take1(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colourSub_filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = colour_entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);
	printf("contexts: %d\n",(int)contextNumber);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats statistics[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		size_t tile_index = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		statistics[entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
		statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
		statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
	for(size_t i=0;i<speed;i++){
		contextNumber = colour_entropy_redistribution_pass(
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = colour_predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);

	size_t available_predictors = 13;

	uint16_t fine_selection[available_predictors] = {
0,
0b0011001011000001,
0b0100000110110001,
0b0100001010110001,
0b0100010010110001,
0b0000001111110010,
0b0001000111000000,
0b0001000111010000,
0b0100001110100001,
0b0011001110110001,
0b0010001011000000,
0b0000000111010000,
0b0001000011010000
	};

	printf("testing %d alternate predictors\n",(int)speed);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			fine_selection[i]
		);
	}


	for(size_t i=0;i<speed + 1;i++){
		contextNumber = colour_entropy_redistribution_pass(
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
	printf("table started\n");
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, range);
	}
	tableEncode.conclude();


	RansEncSymbol esyms[contextNumber][256];

	for(size_t cont=0;cont<contextNumber;cont++){
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms[cont][i], table[cont].cum_freqs[i], table[cont].freqs[i], 16);
		}
	}

	printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		size_t tile_index = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 2]] + filtered_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 1]] + filtered_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 0]] + filtered_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	colour_encode_entropy_channel(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_entropy_channel(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b10000110;
}


#endif //OPTIMISER
