#ifndef COLOUR_OPTIMISER_HEADER
#define COLOUR_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "colour_entropy_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "colour_simple_encoders.hpp"
#include "colour_predictor_optimiser.hpp"
#include "lz_optimiser.hpp"
#include "crossColour_optimiser.hpp"
#include "prefix_coding.hpp"

void colour_optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
);

void colour_encode_combiner(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	size_t safety_margin = 3*width*height * (log2_plus(range - 1) + 1) + 2048;

	uint8_t alternates = 5;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing_end[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing_end[i] = miniBuffer[i] + safety_margin;
		trailing[i] = trailing_end[i];
	}
	colour_encode_entropy(in_bytes,range,width,height,trailing[0]);
	colour_encode_entropy_channel(in_bytes,range,width,height,trailing[1]);
	colour_encode_left(in_bytes,range,width,height,trailing[2]);
	colour_encode_ffv1(in_bytes,range,width,height,trailing[3]);
	colour_optimiser_entropyOnly(in_bytes,range,width,height,trailing[4],2);
/*
	colour_encode_entropy_quad(in_bytes,range,width,height,trailing[4]);*/
	for(size_t i=0;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		//printf("type %d: %d\n",(int)i,(int)diff);
	}

	uint8_t bestIndex = 0;
	size_t best = trailing_end[0] - trailing[0];
	for(size_t i=1;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		if(diff < best){
			best = diff;
			bestIndex = i;
		}
	}
	printf("best type: %d\n",(int)bestIndex);
	for(size_t i=(trailing_end[bestIndex] - trailing[bestIndex]);i--;){
		*(--outPointer) = trailing[bestIndex][i];
	}
	for(size_t i=0;i<alternates;i++){
		delete[] miniBuffer[i];
	}
}

void colour_optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	//printf("initial distribution\n");
	uint8_t contextNumber = colour_entropy_map_initial(
		in_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight,
		0,0,0//use defaults
	);
	//printf("contexts: %d\n",(int)contextNumber);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	//printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

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
		statistics[entropy_image[tile_index*3    ]].freqs[in_bytes[i*3]]++;
		statistics[entropy_image[tile_index*3 + 1]].freqs[in_bytes[i*3 + 1]]++;
		statistics[entropy_image[tile_index*3 + 2]].freqs[in_bytes[i*3 + 2]]++;
	}

	if(contextNumber > 1){
		for(size_t i=0;i<speed;i++){
			contextNumber = colour_entropy_redistribution_pass(
				in_bytes,
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
	}

///encode data
	//printf("table started\n");
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 2]] + in_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 1]] + in_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 0]] + in_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);

	uint8_t* trailing;
	if(contextNumber > 1){
		trailing = outPointer;
		colour_encode_entropy(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		//printf("[entropyOnly] entropy image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	//printf("[entropyOnly] entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	*(--outPointer) = 0b00100010;
}

void colour_optimiser_take0(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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

	size_t available_predictors = 2;

	uint16_t fine_selection[available_predictors] = {
0,
0b0011001011000001
	};

	printf("testing alternate predictor\n");


	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_fast(
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

///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	uint8_t* trailing;

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

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take1(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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

	size_t available_predictors = 4;

	uint16_t fine_selection[available_predictors] = {
0,
0b0011001011000001,
0b0100000110110001,
0b0100001010110001
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_fast(
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
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	uint8_t* trailing;
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

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<3;i++){
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

	for(size_t i=3;i<7;i++){
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

	for(size_t i=7;i<available_predictors;i++){
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
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	uint8_t* trailing;

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

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take3(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 20;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_entropy_channel(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take4(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 64;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011,
0b0000010011100000
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner(
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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take5(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 128;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011,
0b0000010011100000,
0b0100001011000010,
0b0011010010110010,
0b0100001011010001,
0b0001010011010001,
0b0011001110110011,
0b0000000111100000,
0b0001000011100000,
0b0011000111000010,
0b0011001011010000,
0b0001000111110000,
0b0100001110100011,
0b0001000011100001,
0b0100010011010001,
0b0000000111110000,
0b0001001011100000,
0b0011010010110011,
0b0000000011100000,
0b0011010010100001,
0b0100000011100000,
0b0001001111010000,
0b0011001111000010,
0b0100000011010001,
0b0010000011010001,
0b0000001111110000,
0b0010001111010000,
0b0001001011000001,
0b0000001011110001,
0b0000001011100000,
0b0001000011110000,
0b0001001111000001,
0b0010000111110000,
0b0011000111010000,
0b0001001011110000,
0b0010000011100000,
0b0100001110010001,
0b0000010011110001,
0b0001001011000011,
0b0100010010010001,
0b0100001110110010,
0b0010000111100000,
0b0001010011100000,
0b0001010011000011,
0b0010001111010001,
0b0010001011100001,
0b0100001010110011,
0b0000010011010001,
0b0000000111010001,
0b0010010011100000,
0b0011000111100001,
0b0011010011010000,
0b0010001011010001,
0b0011000011010001,
0b0011000011110000,
0b0001000111100001,
0b0010001110110001,
0b0100001111100001,
0b0010010010110011,
0b0001000111000011,
0b0001001111010001,
0b0011000111100000,
0b0001001111110000,
0b0100010011110011,
0b0100010011000000,
0b0001001111000010
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

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

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner(
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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 0b00101110;
}

void colour_optimiser_take3_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 20;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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

//lz interlude
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;

	lz_data = lz_dist_fast(
		in_bytes,
		width,
		height,
		lz_size,
		8
	);

	size_t matchlen_sum = 0;
	for(size_t i=1;i<lz_size;i++){
		matchlen_sum += 1 + lz_data[i].val_matchlen;
	}
	double synth = (double)matchlen_sum/(width*height);
	printf("synth %f\n",synth);

	if(synth > 0.3){
		printf("Trying LZ\n");
		printf("lz size: %d\n",(int)lz_size);
		LZ_used = true;


		for(size_t context = 0;context < contextNumber;context++){
			for(size_t i=0;i<256;i++){
				statistics[context].freqs[i] = 0;
			}
		}

		uint32_t next_match = lz_data[0].val_future;
		size_t lz_looper = 1;
		for(size_t i=0;i<width*height;i++){
			if(next_match == 0){
				i += lz_data[lz_looper].val_matchlen;
				next_match = lz_data[lz_looper++].val_future;
				continue;
			}
			else{
				next_match--;
			}
			size_t tile_index = tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			);
			statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
			statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
			statistics[entropy_image[tile_index*3 + 0]].freqs[filtered_bytes[i*3 + 0]]++;
		}
	}
	else{
		delete[] lz_data;
	}
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	if(LZ_used){
		SymbolStats stats_backref_x;
		SymbolStats stats_backref_y;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref_x.freqs[i] = 0;
			stats_backref_y.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			//printf("%d %d %d %d\n",(int)lz_data[i].backref_x,(int)lz_data[i].backref_y,(int)lz_data[i].matchlen,(int)lz_data[i].future);
			stats_backref_x.freqs[lz_data[i].backref_x]++;
			stats_backref_y.freqs[lz_data[i].backref_y]++;
			stats_matchlen.freqs[lz_data[i].matchlen]++;
			stats_future.freqs[lz_data[i].future]++;
		}
		stats_future.freqs[lz_data[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref_x = encode_freqTable(stats_backref_x, lz_tableEncode, inverse_prefix((width + 1)/2)*2 + 2);
		SymbolStats lz_table_backref_y = encode_freqTable(stats_backref_y, lz_tableEncode, inverse_prefix(height) + 1);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, inverse_prefix(width*height) + 1);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, inverse_prefix(width*height) + 1);
		lz_tableEncode.conclude();
		/*printf(
			"inv pref %d %d %d %d\n",
			(int)(inverse_prefix((width + 1)/2)*2 + 2),
			(int)(inverse_prefix(height) + 1),
			(int)(inverse_prefix(width*height) + 1),
			(int)(inverse_prefix(width*height) + 1)
		);*/

		RansEncSymbol esyms_backref_x[256];
		RansEncSymbol esyms_backref_y[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref_x[i], lz_table_backref_x.cum_freqs[i], lz_table_backref_x.freqs[i], 16);
			RansEncSymbolInit(&esyms_backref_y[i], lz_table_backref_y.cum_freqs[i], lz_table_backref_y.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
			//printf("table: %d %d %d %d\n",(int)lz_table_backref_x.freqs[i],(int)lz_table_backref_y.freqs[i],(int)lz_table_matchlen.freqs[i],(int)lz_table_future.freqs[i]);
		}

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		printf("lz freq tables created\n");

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);

				loose_bytes = lz_data[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_data[lz_size].matchlen);

				loose_bytes = lz_data[lz_size].backref_y_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_y_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_y + lz_data[lz_size].backref_y);

				loose_bytes = lz_data[lz_size].backref_x_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_x_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_x + lz_data[lz_size].backref_x);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
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
		uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_data[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
	}
	else{
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
	}
	delete[] filtered_bytes;

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_entropy_channel(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	if(LZ_used){
		delete[] lz_data;
		*(--outPointer) = 0b00101111;
	}
	else{
		*(--outPointer) = 0b00101110;
	}
}

void colour_optimiser_take4_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 64;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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
//lz interlude
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;

	lz_data = lz_dist_fast(
		in_bytes,
		width,
		height,
		lz_size,
		8
	);

	size_t matchlen_sum = 0;
	for(size_t i=1;i<lz_size;i++){
		matchlen_sum += 1 + lz_data[i].val_matchlen;
	}
	double synth = (double)matchlen_sum/(width*height);

	delete[] lz_data;
	lz_size = 0;

	if(synth > 0.3){
		printf("Trying LZ\n");
		lz_data = lz_dist(
			in_bytes,
			width,
			height,
			lz_size,
			128
		);
		if(lz_size > 1){
			printf("lz size: %d\n",(int)lz_size);
			LZ_used = true;


			for(size_t context = 0;context < contextNumber;context++){
				for(size_t i=0;i<256;i++){
					statistics[context].freqs[i] = 0;
				}
			}

			uint32_t next_match = lz_data[0].val_future;
			size_t lz_looper = 1;
			for(size_t i=0;i<width*height;i++){
				if(next_match == 0){
					i += lz_data[lz_looper].val_matchlen;
					next_match = lz_data[lz_looper++].val_future;
					continue;
				}
				else{
					next_match--;
				}
				size_t tile_index = tileIndexFromPixel(
					i,
					width,
					entropyWidth,
					entropyWidth_block,
					entropyHeight_block
				);
				statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
				statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
				statistics[entropy_image[tile_index*3 + 0]].freqs[filtered_bytes[i*3 + 0]]++;
			}
		}
		else{
			delete[] lz_data;
		}
	}
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	if(LZ_used){
		SymbolStats stats_backref_x;
		SymbolStats stats_backref_y;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref_x.freqs[i] = 0;
			stats_backref_y.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			//printf("%d %d %d %d\n",(int)lz_data[i].backref_x,(int)lz_data[i].backref_y,(int)lz_data[i].matchlen,(int)lz_data[i].future);
			stats_backref_x.freqs[lz_data[i].backref_x]++;
			stats_backref_y.freqs[lz_data[i].backref_y]++;
			stats_matchlen.freqs[lz_data[i].matchlen]++;
			stats_future.freqs[lz_data[i].future]++;
		}
		stats_future.freqs[lz_data[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref_x = encode_freqTable(stats_backref_x, lz_tableEncode, inverse_prefix((width + 1)/2)*2 + 2);
		SymbolStats lz_table_backref_y = encode_freqTable(stats_backref_y, lz_tableEncode, inverse_prefix(height) + 1);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, inverse_prefix(width*height) + 1);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, inverse_prefix(width*height) + 1);
		lz_tableEncode.conclude();
		/*printf(
			"inv pref %d %d %d %d\n",
			(int)(inverse_prefix((width + 1)/2)*2 + 2),
			(int)(inverse_prefix(height) + 1),
			(int)(inverse_prefix(width*height) + 1),
			(int)(inverse_prefix(width*height) + 1)
		);*/

		RansEncSymbol esyms_backref_x[256];
		RansEncSymbol esyms_backref_y[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref_x[i], lz_table_backref_x.cum_freqs[i], lz_table_backref_x.freqs[i], 16);
			RansEncSymbolInit(&esyms_backref_y[i], lz_table_backref_y.cum_freqs[i], lz_table_backref_y.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
			//printf("table: %d %d %d %d\n",(int)lz_table_backref_x.freqs[i],(int)lz_table_backref_y.freqs[i],(int)lz_table_matchlen.freqs[i],(int)lz_table_future.freqs[i]);
		}

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		printf("lz freq tables created\n");

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);

				loose_bytes = lz_data[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_data[lz_size].matchlen);

				loose_bytes = lz_data[lz_size].backref_y_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_y_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_y + lz_data[lz_size].backref_y);

				loose_bytes = lz_data[lz_size].backref_x_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_x_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_x + lz_data[lz_size].backref_x);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
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
		uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_data[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
	}
	else{
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
	}
	delete[] filtered_bytes;

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner(
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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	if(LZ_used){
		delete[] lz_data;
		*(--outPointer) = 0b00101111;
	}
	else{
		*(--outPointer) = 0b00101110;
	}
}

void colour_optimiser_take5_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 128;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011,
0b0000010011100000,
0b0100001011000010,
0b0011010010110010,
0b0100001011010001,
0b0001010011010001,
0b0011001110110011,
0b0000000111100000,
0b0001000011100000,
0b0011000111000010,
0b0011001011010000,
0b0001000111110000,
0b0100001110100011,
0b0001000011100001,
0b0100010011010001,
0b0000000111110000,
0b0001001011100000,
0b0011010010110011,
0b0000000011100000,
0b0011010010100001,
0b0100000011100000,
0b0001001111010000,
0b0011001111000010,
0b0100000011010001,
0b0010000011010001,
0b0000001111110000,
0b0010001111010000,
0b0001001011000001,
0b0000001011110001,
0b0000001011100000,
0b0001000011110000,
0b0001001111000001,
0b0010000111110000,
0b0011000111010000,
0b0001001011110000,
0b0010000011100000,
0b0100001110010001,
0b0000010011110001,
0b0001001011000011,
0b0100010010010001,
0b0100001110110010,
0b0010000111100000,
0b0001010011100000,
0b0001010011000011,
0b0010001111010001,
0b0010001011100001,
0b0100001010110011,
0b0000010011010001,
0b0000000111010001,
0b0010010011100000,
0b0011000111100001,
0b0011010011010000,
0b0010001011010001,
0b0011000011010001,
0b0011000011110000,
0b0001000111100001,
0b0010001110110001,
0b0100001111100001,
0b0010010010110011,
0b0001000111000011,
0b0001001111010001,
0b0011000111100000,
0b0001001111110000,
0b0100010011110011,
0b0100010011000000,
0b0001001111000010
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

//one pass for stats tables
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
//lz interlude
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;

	lz_data = lz_dist_fast(
		in_bytes,
		width,
		height,
		lz_size,
		16
	);

	size_t matchlen_sum = 0;
	for(size_t i=1;i<lz_size;i++){
		matchlen_sum += 1 + lz_data[i].val_matchlen;
	}
	double synth = (double)matchlen_sum/(width*height);

	delete[] lz_data;
	lz_size = 0;

	if(synth > 0.25){
		printf("Trying LZ\n");
		lz_data = lz_dist(
			in_bytes,
			width,
			height,
			lz_size,
			256
		);
		if(lz_size > 1){
			printf("lz size: %d\n",(int)lz_size);
			LZ_used = true;


			for(size_t context = 0;context < contextNumber;context++){
				for(size_t i=0;i<256;i++){
					statistics[context].freqs[i] = 0;
				}
			}

			uint32_t next_match = lz_data[0].val_future;
			size_t lz_looper = 1;
			for(size_t i=0;i<width*height;i++){
				if(next_match == 0){
					i += lz_data[lz_looper].val_matchlen;
					next_match = lz_data[lz_looper++].val_future;
					continue;
				}
				else{
					next_match--;
				}
				size_t tile_index = tileIndexFromPixel(
					i,
					width,
					entropyWidth,
					entropyWidth_block,
					entropyHeight_block
				);
				statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
				statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
				statistics[entropy_image[tile_index*3 + 0]].freqs[filtered_bytes[i*3 + 0]]++;
			}
/*
			BitWriter tableEncode2;
			tableEncode = tableEncode2;
			for(size_t context = 0;context < contextNumber;context++){
				table[context] = encode_freqTable(statistics[context], tableEncode, range);
			}
			tableEncode.conclude();
*/
		}
		else{
			delete[] lz_data;
		}
	}
///encode data
	//printf("table started\n");
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

	//printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	if(LZ_used){
		SymbolStats stats_backref_x;
		SymbolStats stats_backref_y;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref_x.freqs[i] = 0;
			stats_backref_y.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			//printf("%d %d %d %d\n",(int)lz_data[i].backref_x,(int)lz_data[i].backref_y,(int)lz_data[i].matchlen,(int)lz_data[i].future);
			stats_backref_x.freqs[lz_data[i].backref_x]++;
			stats_backref_y.freqs[lz_data[i].backref_y]++;
			stats_matchlen.freqs[lz_data[i].matchlen]++;
			stats_future.freqs[lz_data[i].future]++;
		}
		stats_future.freqs[lz_data[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref_x = encode_freqTable(stats_backref_x, lz_tableEncode, inverse_prefix((width + 1)/2)*2 + 2);
		SymbolStats lz_table_backref_y = encode_freqTable(stats_backref_y, lz_tableEncode, inverse_prefix(height) + 1);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, inverse_prefix(width*height) + 1);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, inverse_prefix(width*height) + 1);
		lz_tableEncode.conclude();
		/*printf(
			"inv pref %d %d %d %d\n",
			(int)(inverse_prefix((width + 1)/2)*2 + 2),
			(int)(inverse_prefix(height) + 1),
			(int)(inverse_prefix(width*height) + 1),
			(int)(inverse_prefix(width*height) + 1)
		);*/

		RansEncSymbol esyms_backref_x[256];
		RansEncSymbol esyms_backref_y[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref_x[i], lz_table_backref_x.cum_freqs[i], lz_table_backref_x.freqs[i], 16);
			RansEncSymbolInit(&esyms_backref_y[i], lz_table_backref_y.cum_freqs[i], lz_table_backref_y.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
			//printf("table: %d %d %d %d\n",(int)lz_table_backref_x.freqs[i],(int)lz_table_backref_y.freqs[i],(int)lz_table_matchlen.freqs[i],(int)lz_table_future.freqs[i]);
		}

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		printf("lz freq tables created\n");

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);

				loose_bytes = lz_data[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_data[lz_size].matchlen);

				loose_bytes = lz_data[lz_size].backref_y_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_y_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_y + lz_data[lz_size].backref_y);

				loose_bytes = lz_data[lz_size].backref_x_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_x_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_x + lz_data[lz_size].backref_x);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
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
		uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_data[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
	}
	else{
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
	}
	delete[] filtered_bytes;

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner(
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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	if(LZ_used){
		delete[] lz_data;
		*(--outPointer) = 0b00101111;
	}
	else{
		*(--outPointer) = 0b00101110;
	}
}

void colour_optimiser_take6_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

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
		entropyHeight,
		0,0,0//use defaults
	);
	printf("contexts: %d\n",(int)contextNumber);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats* statistics = new SymbolStats[contextNumber];
	
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

	printf("performing 6 entropy passes\n");
	for(size_t i=0;i<6;i++){
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
	uint8_t** filter_collection = new uint8_t*[256];
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
	filter_collection[0] = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	size_t available_predictors = 128;

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
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011,
0b0000010011100000,
0b0100001011000010,
0b0011010010110010,
0b0100001011010001,
0b0001010011010001,
0b0011001110110011,
0b0000000111100000,
0b0001000011100000,
0b0011000111000010,
0b0011001011010000,
0b0001000111110000,
0b0100001110100011,
0b0001000011100001,
0b0100010011010001,
0b0000000111110000,
0b0001001011100000,
0b0011010010110011,
0b0000000011100000,
0b0011010010100001,
0b0100000011100000,
0b0001001111010000,
0b0011001111000010,
0b0100000011010001,
0b0010000011010001,
0b0000001111110000,
0b0010001111010000,
0b0001001011000001,
0b0000001011110001,
0b0000001011100000,
0b0001000011110000,
0b0001001111000001,
0b0010000111110000,
0b0011000111010000,
0b0001001011110000,
0b0010000011100000,
0b0100001110010001,
0b0000010011110001,
0b0001001011000011,
0b0100010010010001,
0b0100001110110010,
0b0010000111100000,
0b0001010011100000,
0b0001010011000011,
0b0010001111010001,
0b0010001011100001,
0b0100001010110011,
0b0000010011010001,
0b0000000111010001,
0b0010010011100000,
0b0011000111100001,
0b0011010011010000,
0b0010001011010001,
0b0011000011010001,
0b0011000011110000,
0b0001000111100001,
0b0010001110110001,
0b0100001111100001,
0b0010010010110011,
0b0001000111000011,
0b0001001111010001,
0b0011000111100000,
0b0001001111110000,
0b0100010011110011,
0b0100010011000000,
0b0001001111000010
	};

	printf("testing %d alternate predictors\n",(int)available_predictors);

	for(size_t i=1;i<available_predictors;i++){
		predictorCount = colourSub_add_predictor_maybe_prefiltered(
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
			fine_selection[i],
			filter_collection
		);
	}

	printf("performing %d refinement passes\n",(int)speed);
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

		//printf("shuffling predictors around\n");
		double saved = colourSub_predictor_redistribution_pass_prefiltered(
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
			filter_collection
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}
//
	contextNumber = colour_contextSize_optimiser(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		entropyWidth_block,
		entropyHeight_block,
		statistics,
		speed
	);
//one pass for stats tables
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
//lz interlude
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;

	lz_data = lz_dist_fast(
		in_bytes,
		width,
		height,
		lz_size,
		32
	);

	size_t matchlen_sum = 0;
	for(size_t i=1;i<lz_size;i++){
		matchlen_sum += 1 + lz_data[i].val_matchlen;
	}
	double synth = (double)matchlen_sum/(width*height);

	delete[] lz_data;
	lz_size = 0;
	if(synth > 0.25){
		printf("Trying LZ\n");
		lz_data = lz_dist(
			in_bytes,
			width,
			height,
			lz_size,
			width*3
		);
		/*lz_data = lz_dist_fast(
			in_bytes,
			width,
			height,
			lz_size
		);*/
		if(lz_size > 1){
			printf("lz size: %d\n",(int)lz_size);
			LZ_used = true;


			for(size_t context = 0;context < contextNumber;context++){
				for(size_t i=0;i<256;i++){
					statistics[context].freqs[i] = 0;
				}
			}

			uint32_t next_match = lz_data[0].val_future;
			size_t lz_looper = 1;
			for(size_t i=0;i<width*height;i++){
				if(next_match == 0){
					i += lz_data[lz_looper].val_matchlen;
					next_match = lz_data[lz_looper++].val_future;
					continue;
				}
				else{
					next_match--;
				}
				size_t tile_index = tileIndexFromPixel(
					i,
					width,
					entropyWidth,
					entropyWidth_block,
					entropyHeight_block
				);
				statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
				statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
				statistics[entropy_image[tile_index*3 + 0]].freqs[filtered_bytes[i*3 + 0]]++;
			}
/*
			BitWriter tableEncode2;
			tableEncode = tableEncode2;
			for(size_t context = 0;context < contextNumber;context++){
				table[context] = encode_freqTable(statistics[context], tableEncode, range);
			}
			tableEncode.conclude();
*/
		}
		else{
			delete[] lz_data;
		}
	}
///encode data
	//printf("table started\n");
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, range);
	}
	tableEncode.conclude();
	delete[] statistics;


	RansEncSymbol esyms[contextNumber][256];

	for(size_t cont=0;cont<contextNumber;cont++){
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms[cont][i], table[cont].cum_freqs[i], table[cont].freqs[i], 16);
		}
	}

	//printf("ransenc\n");

	RansState rans;
	RansEncInit(&rans);
	if(LZ_used){
		SymbolStats stats_backref_x;
		SymbolStats stats_backref_y;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref_x.freqs[i] = 0;
			stats_backref_y.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			//printf("%d %d %d %d\n",(int)lz_data[i].backref_x,(int)lz_data[i].backref_y,(int)lz_data[i].matchlen,(int)lz_data[i].future);
			stats_backref_x.freqs[lz_data[i].backref_x]++;
			stats_backref_y.freqs[lz_data[i].backref_y]++;
			stats_matchlen.freqs[lz_data[i].matchlen]++;
			stats_future.freqs[lz_data[i].future]++;
		}
		stats_future.freqs[lz_data[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref_x = encode_freqTable(stats_backref_x, lz_tableEncode, inverse_prefix((width + 1)/2)*2 + 2);
		SymbolStats lz_table_backref_y = encode_freqTable(stats_backref_y, lz_tableEncode, inverse_prefix(height) + 1);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, inverse_prefix(width*height) + 1);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, inverse_prefix(width*height) + 1);
		lz_tableEncode.conclude();
		/*printf(
			"inv pref %d %d %d %d\n",
			(int)(inverse_prefix((width + 1)/2)*2 + 2),
			(int)(inverse_prefix(height) + 1),
			(int)(inverse_prefix(width*height) + 1),
			(int)(inverse_prefix(width*height) + 1)
		);*/

		RansEncSymbol esyms_backref_x[256];
		RansEncSymbol esyms_backref_y[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref_x[i], lz_table_backref_x.cum_freqs[i], lz_table_backref_x.freqs[i], 16);
			RansEncSymbolInit(&esyms_backref_y[i], lz_table_backref_y.cum_freqs[i], lz_table_backref_y.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
			//printf("table: %d %d %d %d\n",(int)lz_table_backref_x.freqs[i],(int)lz_table_backref_y.freqs[i],(int)lz_table_matchlen.freqs[i],(int)lz_table_future.freqs[i]);
		}

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		printf("lz freq tables created\n");

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);

				loose_bytes = lz_data[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_data[lz_size].matchlen);

				loose_bytes = lz_data[lz_size].backref_y_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_y_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_y + lz_data[lz_size].backref_y);

				loose_bytes = lz_data[lz_size].backref_x_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_data[lz_size].backref_x_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref_x + lz_data[lz_size].backref_x);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
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
		uint8_t loose_bytes = lz_data[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_data[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_data[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
	}
	else{
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
	}
	delete[] filtered_bytes;

	//printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	colour_encode_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner(
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

	trailing = outPointer;
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	if(LZ_used){
		delete[] lz_data;
		*(--outPointer) = 0b00101111;
	}
	else{
		*(--outPointer) = 0b00101110;
	}
}

#endif //COLOUR_OPTIMISER
