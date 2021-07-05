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

void colour_simple_combiner(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer,bool debug){
	uint8_t* trailing = outPointer;
	colour_encode_entropy_channel(in_bytes,range,width,height,outPointer,debug);
	if(trailing - outPointer >= 4 + width*height*3){
		if(debug){
			printf("no type better than raw pixels\n");
		}
		outPointer = trailing;
		colour_encode_raw(in_bytes,range,width,height,outPointer);
	}
	
}

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
	colour_encode_entropy_channel(in_bytes,range,width,height,trailing[1],false);
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
	if(best >= 4 + width*height*3){
		printf("no type better than raw pixels\n");
	}
	printf("best type: %d\n",(int)bestIndex);
	for(size_t i=(trailing_end[bestIndex] - trailing[bestIndex]);i--;){
		*(--outPointer) = trailing[bestIndex][i];
	}
	for(size_t i=0;i<alternates;i++){
		delete[] miniBuffer[i];
	}
}

void colour_optimiser_take1(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
);

void colour_optimiser_take2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
);

void colour_optimiser_take3_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
);

void colour_encode_combiner_slow(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	size_t safety_margin = 3*width*height * (log2_plus(range - 1) + 1 + 1) + 2048;

	uint8_t alternates = 8;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing_end[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing_end[i] = miniBuffer[i] + safety_margin;
		trailing[i] = trailing_end[i];
	}
	colour_encode_entropy(in_bytes,range,width,height,trailing[0]);
	colour_encode_entropy_channel(in_bytes,range,width,height,trailing[1],false);
	colour_encode_left(in_bytes,range,width,height,trailing[2]);
	colour_encode_ffv1(in_bytes,range,width,height,trailing[3]);
	colour_optimiser_entropyOnly(in_bytes,range,width,height,trailing[4],2);
	colour_optimiser_take1(
		in_bytes,
		range,
		width,
		height,
		trailing[5],
		1,false
	);
	colour_optimiser_take2(
		in_bytes,
		range,
		width,
		height,
		trailing[6],
		5,false
	);
	colour_optimiser_take3_lz(
		in_bytes,
		range,
		width,
		height,
		trailing[7],
		5,false
	);
/*
	colour_encode_entropy_quad(in_bytes,range,width,height,trailing[4]);*/
	for(size_t i=0;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		//printf("type %d: %d\n",(int)i,(int)diff);
	}

	uint8_t bestIndex = 0;
	size_t best = trailing_end[0] - trailing[0];
	printf("  type 0: %d bytes\n",(int)best);
	for(size_t i=1;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		printf("  type %d: %d bytes\n",(int)i,(int)diff);
		if(diff < best){
			best = diff;
			bestIndex = i;
		}
	}
	if(best >= 4 + width*height*3){
		printf("no type better than raw pixels\n");
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

	*(--outPointer) = 0b01000010;
}

void colour_optimiser_take0(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
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

	printf("setting initial predictor layout\n");

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
			fine_selection[i],
			debug
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
	colour_simple_combiner(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer,
		false
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
		colour_simple_combiner(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer,
			false
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
	*(--outPointer) = 0b01100110;
}

void colour_optimiser_take1(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
){
	if(debug){
		printf("pre-pre-range: %d\n",(int)range);
		for(size_t i=0;i<width*height;i++){
			if(in_bytes[i*3] >= range){
				printf("OH NO!!!!!!!!!!!!\n");
			}
			if(in_bytes[i*3 + 1] >= range){
				printf("1OH NO!!!!!!!!!!!!\n");
			}
			if(in_bytes[i*3 + 2] >= range){
				printf("2OH NO!!!!!!!!!!!!\n");
			}
		}
	}
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);
	if(debug){
		printf("pre-range: %d\n",(int)range);
		for(size_t i=0;i<width*height;i++){
			if(filtered_bytes[i*3] >= range){
				printf("OH NO!!!!!!!!!!!!\n");
			}
			if(filtered_bytes[i*3 + 1] >= range){
				printf("1OH NO!!!!!!!!!!!!\n");
			}
			if(filtered_bytes[i*3 + 2] >= range){
				printf("2OH NO!!!!!!!!!!!!\n");
			}
		}
	}

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	if(debug){
		printf("initial distribution\n");
	}
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
	if(debug){
		printf("contexts: %d\n",(int)contextNumber);
	}

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	if(debug){
		printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	}

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
	if(debug){
		printf("performing %d entropy passes\n",(int)speed);
	}
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
	if(debug){
		printf("setting initial predictor layout\n");
	}

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

	if(debug){
		printf("testing %d alternate predictors\n",(int)available_predictors);
	}

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
			fine_selection[i],
			debug
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
	if(debug){
		printf("table started: %d\n",(int)contextNumber);
	}
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 2]] + filtered_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 1]] + filtered_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 0]] + filtered_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	if(debug){
		printf("ransenc done\n");
	}

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		colour_simple_combiner(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer,
			false
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		if(debug){
			printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	if(debug){
		printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));
	}

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_simple_combiner(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer,
			false
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		if(debug){
			printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b01100110;
}

void colour_optimiser_take2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	if(debug){
		printf("initial distribution\n");
	}
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
	if(debug){
		printf("contexts: %d\n",(int)contextNumber);
	}

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	if(debug){
		printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	}

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

	if(debug){
		printf("performing %d entropy passes\n",(int)speed);
	}
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
	if(debug){
		printf("setting initial predictor layout\n");
	}

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

	if(debug){
		printf("testing %d alternate predictors\n",(int)available_predictors);
	}

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
			fine_selection[i],
			debug
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
			fine_selection[i],
			debug
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
			fine_selection[i],
			debug
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
	if(debug){
		printf("table started\n");
	}
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

	if(debug){
		printf("ransenc\n");
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 2]] + filtered_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 1]] + filtered_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index*3 + 0]] + filtered_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	if(debug){
		printf("ransenc done\n");
	}

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		colour_simple_combiner(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer,
			false
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		if(debug){
			printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	if(debug){
		printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));
	}

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_simple_combiner(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer,
			false
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		if(debug){
			printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b01100110;
}

void colour_optimiser_take3_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
){
	uint8_t* filtered_bytes = colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	if(debug){
		printf("initial distribution\n");
	}
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
	if(debug){
		printf("contexts: %d\n",(int)contextNumber);
	}

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	if(debug){
		printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	}

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

	if(debug){
		printf("performing %d entropy passes\n",(int)speed);
	}
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

	if(debug){
		printf("setting initial predictor layout\n");
	}

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

	if(debug){
		printf("testing %d alternate predictors\n",(int)available_predictors);
	}

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
			filter_collection,
			debug
		);
	}

	if(debug){
		printf("performing %d refinement passes\n",(int)speed);
	}
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
		if(debug){
			printf("saved: %f bits\n",saved);
		}
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
	if(debug){
		printf("synth %f\n",synth);
	}
	lz_triple_c* lz_symbols;

	if(synth > 0.3){
		if(debug){
			printf("Trying LZ\n");
			printf("lz size: %d\n",(int)lz_size);
		}
		LZ_used = true;

		lz_symbols = lz_translator(
			lz_data,
			width,
			lz_size
		);


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
		SymbolStats stats_backref;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			stats_backref.freqs[lz_symbols[i].backref]++;
			stats_matchlen.freqs[lz_symbols[i].matchlen]++;
			stats_future.freqs[lz_symbols[i].future]++;
		}
		stats_future.freqs[lz_symbols[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref  = encode_freqTable(stats_backref,  lz_tableEncode, 200);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 40);
		SymbolStats lz_table_future   = encode_freqTable(stats_future,   lz_tableEncode, 40);
		lz_tableEncode.conclude();

		RansEncSymbol esyms_backref[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref[i],   lz_table_backref.cum_freqs[i],   lz_table_backref.freqs[i],   16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
		}

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		if(debug){
			printf("lz freq tables created\n");
		}

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);

				loose_bytes = lz_symbols[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_symbols[lz_size].matchlen);

				loose_bytes = lz_symbols[lz_size].backref_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].backref_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref + lz_symbols[lz_size].backref);

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
		uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_symbols[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);
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

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		colour_simple_combiner(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer,
			false
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		if(debug){
			printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	if(debug){
		printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));
	}

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_simple_combiner(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer,
			true
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		if(debug){
			printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
		}
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;
	if(debug){
		printf("%d predictors\n",(int)predictorCount);
	}

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01100111;
	}
	else{
		*(--outPointer) = 0b01100110;
	}
}

void colour_optimiser_take4_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
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

	printf("setting initial predictor layout\n");

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
			filter_collection,
			debug
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
	lz_triple_c* lz_symbols;

	if(synth > 0.3){
		printf("Trying LZ\n");
		lz_data = lz_dist(
			in_bytes,
			width,
			height,
			lz_size,
			128
		);
		lz_symbols = lz_translator(
			lz_data,
			width,
			lz_size
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
			delete[] lz_symbols;
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
		SymbolStats stats_backref;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			stats_backref.freqs[lz_symbols[i].backref]++;
			stats_matchlen.freqs[lz_symbols[i].matchlen]++;
			stats_future.freqs[lz_symbols[i].future]++;
		}
		stats_future.freqs[lz_symbols[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref  = encode_freqTable(stats_backref,  lz_tableEncode, 200);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 40);
		SymbolStats lz_table_future   = encode_freqTable(stats_future,   lz_tableEncode, 40);
		lz_tableEncode.conclude();

		RansEncSymbol esyms_backref[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref[i],   lz_table_backref.cum_freqs[i],   lz_table_backref.freqs[i],   16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
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

				uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);

				loose_bytes = lz_symbols[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_symbols[lz_size].matchlen);

				loose_bytes = lz_symbols[lz_size].backref_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].backref_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref + lz_symbols[lz_size].backref);

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
		uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_symbols[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);
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

	uint8_t* trailing  = outPointer;
	if(contextNumber > 1){
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
	}
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

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01100111;
	}
	else{
		*(--outPointer) = 0b01100110;
	}
}

void colour_optimiser_take5_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
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

	printf("setting initial predictor layout\n");

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
			filter_collection,
			debug
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
	lz_triple_c* lz_symbols;

	if(synth > 0.25){
		printf("Trying LZ\n");
		lz_data = lz_dist(
			in_bytes,
			width,
			height,
			lz_size,
			256
		);
		lz_symbols = lz_translator(
			lz_data,
			width,
			lz_size
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
			delete[] lz_symbols;
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
		SymbolStats stats_backref;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			stats_backref.freqs[lz_symbols[i].backref]++;
			stats_matchlen.freqs[lz_symbols[i].matchlen]++;
			stats_future.freqs[lz_symbols[i].future]++;
		}
		stats_future.freqs[lz_symbols[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref  = encode_freqTable(stats_backref,  lz_tableEncode, 200);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 40);
		SymbolStats lz_table_future   = encode_freqTable(stats_future,   lz_tableEncode, 40);
		lz_tableEncode.conclude();

		RansEncSymbol esyms_backref[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref[i],   lz_table_backref.cum_freqs[i],   lz_table_backref.freqs[i],   16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
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

				uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);

				loose_bytes = lz_symbols[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_symbols[lz_size].matchlen);

				loose_bytes = lz_symbols[lz_size].backref_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].backref_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				RansEncPutSymbol(&rans, &outPointer, esyms_backref + lz_symbols[lz_size].backref);

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
		uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_symbols[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);
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

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
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
	}
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

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01100111;
	}
	else{
		*(--outPointer) = 0b01100110;
	}
}

void colour_optimiser_take6_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
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

	printf("setting initial predictor layout\n");

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

	printf("testing %d alternate predictors",(int)available_predictors);

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
			filter_collection,
			debug
		);
	}

	printf("\nperforming %d refinement passes\n",(int)speed/2);
	for(size_t i=0;i<speed/2;i++){
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
/*
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
*/
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
	printf("Testing LZ\n");
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;
	float* estimate = new float[width*height];

	double* costTables[contextNumber];

	for(size_t i=0;i<contextNumber;i++){
		costTables[i] = entropyLookup(statistics[i]);
	}

	for(size_t i=0;i<width*height;i++){
		size_t tile_index = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		estimate[i] =
			costTables[entropy_image[tile_index*3]][filtered_bytes[i*3]]
			+ costTables[entropy_image[tile_index*3+1]][filtered_bytes[i*3+1]]
			+ costTables[entropy_image[tile_index*3+2]][filtered_bytes[i*3+2]];
	}

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}


//
	double* backref_cost_table;
	double* matchlen_cost_table;
	double* future_cost_table;
	lz_initial_cost(
		width,
		height,
		backref_cost_table,
		matchlen_cost_table,
		future_cost_table
	);
	lz_data = lz_dist_modern(
		in_bytes,
		estimate,
		width,
		height,
		backref_cost_table,
		matchlen_cost_table,
		future_cost_table,
		lz_size,
		20
	);
	printf("lz size: %d\n",(int)lz_size);
	delete[] backref_cost_table;
	delete[] matchlen_cost_table;
	delete[] future_cost_table;
//
	lz_triple_c* lz_symbols;
	if(lz_size > 10){
		for(size_t i=0;i<predictorCount;i++){
			delete[] filter_collection[i];
		}
		delete[] filter_collection;
		printf("Trying LZ\n");
		size_t lzlimit;
		if(speed > 18){
			lzlimit = 4 << 18;
		}
		else{
			lzlimit = 4 << speed;
		}
		lz_symbols = lz_pruner(
			estimate,
			width,
			lz_data,
			lz_size
		);
		lz_cost(
			lz_symbols,
			lz_size,
			backref_cost_table,
			matchlen_cost_table,
			future_cost_table
		);

		lz_size = 0;
		delete[] lz_data;
		lz_data = lz_dist_modern(
			in_bytes,
			estimate,
			width,
			height,
			backref_cost_table,
			matchlen_cost_table,
			future_cost_table,
			lz_size,
			120
		);
		printf("lz size: %d\n",(int)lz_size);
		delete[] backref_cost_table;
		delete[] matchlen_cost_table;
		delete[] future_cost_table;
		delete[] lz_symbols;
		lz_symbols = lz_pruner(
			estimate,
			width,
			lz_data,
			lz_size
		);
		lz_cost(
			lz_symbols,
			lz_size,
			backref_cost_table,
			matchlen_cost_table,
			future_cost_table
		);

		if(speed > 8){
			lz_size = 0;
			delete[] lz_data;
			lz_data = lz_dist_modern(
				in_bytes,
				estimate,
				width,
				height,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table,
				lz_size,
				(4 << 8)
			);
			printf("lz size: %d\n",(int)lz_size);
			delete[] backref_cost_table;
			delete[] matchlen_cost_table;
			delete[] future_cost_table;
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
			lz_cost(
				lz_symbols,
				lz_size,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table
			);
		}
		if(speed > 9){
			lz_size = 0;
			delete[] lz_data;
			lz_data = lz_dist_modern(
				in_bytes,
				estimate,
				width,
				height,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table,
				lz_size,
				(4 << 9)
			);
			printf("lz size: %d\n",(int)lz_size);
			delete[] backref_cost_table;
			delete[] matchlen_cost_table;
			delete[] future_cost_table;
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
			lz_cost(
				lz_symbols,
				lz_size,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table
			);
		}
		if(speed > 10){
			lz_size = 0;
			delete[] lz_data;
			lz_data = lz_dist_modern(
				in_bytes,
				estimate,
				width,
				height,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table,
				lz_size,
				(4 << 10)
			);
			printf("lz size: %d\n",(int)lz_size);
			delete[] backref_cost_table;
			delete[] matchlen_cost_table;
			delete[] future_cost_table;
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
			lz_cost(
				lz_symbols,
				lz_size,
				backref_cost_table,
				matchlen_cost_table,
				future_cost_table
			);
		}
		/*for(size_t i=0;i<256;i++){
			printf("cost %d: %f\n",(int)i,backref_cost_table[i]);
		}*/
		lz_size = 0;
		delete[] lz_data;
		lz_data = lz_dist_modern(
			in_bytes,
			estimate,
			width,
			height,
			backref_cost_table,
			matchlen_cost_table,
			future_cost_table,
			lz_size,
			lzlimit
		);
		delete[] backref_cost_table;
		delete[] matchlen_cost_table;
		delete[] future_cost_table;

		printf("lz size: %d\n",(int)lz_size);
		size_t old_lz_size = lz_size;
		delete[] lz_symbols;
		lz_symbols = lz_pruner(
			estimate,
			width,
			lz_data,
			lz_size
		);
		if(lz_size && old_lz_size/lz_size < 10){
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
		}
		if(lz_size > 10){
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
			if(speed > 9){
				double* costTables[contextNumber];

				for(size_t i=0;i<contextNumber;i++){
					costTables[i] = entropyLookup(statistics[i]);
				}

				for(size_t i=0;i<width*height;i++){
					size_t tile_index = tileIndexFromPixel(
						i,
						width,
						entropyWidth,
						entropyWidth_block,
						entropyHeight_block
					);
					estimate[i] =
						costTables[entropy_image[tile_index*3]][filtered_bytes[i*3]]
						+ costTables[entropy_image[tile_index*3+1]][filtered_bytes[i*3+1]]
						+ costTables[entropy_image[tile_index*3+2]][filtered_bytes[i*3+2]];
				}

				for(size_t i=0;i<contextNumber;i++){
					delete[] costTables[i];
				}
				printf("Doing introspective LZ matching (slow). Only enabled at speed > 9\n");
				lz_cost(
					lz_symbols,
					lz_size,
					backref_cost_table,
					matchlen_cost_table,
					future_cost_table
				);
				lz_size = 0;
				delete[] lz_data;
				lz_data = lz_dist_modern(
					in_bytes,
					estimate,
					width,
					height,
					backref_cost_table,
					matchlen_cost_table,
					future_cost_table,
					lz_size,
					lzlimit
				);
				printf("lz size: %d\n",(int)lz_size);
				delete[] backref_cost_table;
				delete[] matchlen_cost_table;
				delete[] future_cost_table;
				delete[] lz_symbols;
				lz_symbols = lz_pruner(
					estimate,
					width,
					lz_data,
					lz_size
				);

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
		}
		else{
			delete[] lz_data;
		}
		delete[] estimate;
	}
	else{
		delete[] lz_data;
		delete[] estimate;
		printf("performing %d refinement passes\n",(int)(speed - speed/2));
		for(size_t i=0;i<speed - speed/2;i++){
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
		for(size_t i=0;i<predictorCount;i++){
			delete[] filter_collection[i];
		}
		delete[] filter_collection;
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
		SymbolStats stats_backref;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			stats_backref.freqs[lz_symbols[i].backref]++;
			stats_matchlen.freqs[lz_symbols[i].matchlen]++;
			stats_future.freqs[lz_symbols[i].future]++;
		}
		stats_future.freqs[lz_symbols[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref = encode_freqTable(stats_backref, lz_tableEncode, 200);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 40);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, 40);
		lz_tableEncode.conclude();

		RansEncSymbol esyms_backref[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref[i], lz_table_backref.cum_freqs[i], lz_table_backref.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
		}

		double backref_cost = 0;
		double matchlen_cost = 0;
		double future_cost = 0;

		for(size_t i=0;i<40;i++){
			uint8_t extrabits = extrabits_from_prefix(i);
			if(lz_table_matchlen.freqs[i]){
				matchlen_cost += stats_matchlen.freqs[i] * (extrabits - std::log2((double)lz_table_matchlen.freqs[i]) + 16);
			}
			if(lz_table_future.freqs[i]){
				future_cost += stats_future.freqs[i] * (extrabits - std::log2((double)lz_table_future.freqs[i]) + 16);
			}
			if(lz_table_backref.freqs[160+i]){
				backref_cost += stats_backref.freqs[160+i] * (extrabits - std::log2((double)lz_table_backref.freqs[160+i]) + 16);
			}
		}
		for(size_t i=0;i<120;i++){
			if(lz_table_backref.freqs[i]){
				backref_cost += stats_backref.freqs[i] * (-std::log2((double)lz_table_backref.freqs[i]) + 16);
			}
		}
		for(size_t i=0;i<20;i++){
			uint8_t extrabits = extrabits_from_prefix(i);
			if(lz_table_backref.freqs[120+i]){
				backref_cost += stats_backref.freqs[i] * (extrabits - std::log2((double)lz_table_backref.freqs[120+i]) + 16 + 5);
			}
			if(lz_table_backref.freqs[140+i]){
				backref_cost += stats_backref.freqs[i] * (extrabits - std::log2((double)lz_table_backref.freqs[140+i]) + 16);
			}
		}
		printf("LZ: backref %f bytes, matchlen: %f bytes, offset: %f bytes\n",backref_cost/8,matchlen_cost/8,future_cost/8);

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			//printf("%d\n",(int)index);
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("a %d\n",(int)index);
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);

				loose_bytes = lz_symbols[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("b %d %d\n",(int)index,(int)lz_symbols[lz_size].matchlen);
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_symbols[lz_size].matchlen);

				loose_bytes = lz_symbols[lz_size].backref_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].backref_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("c %d %d\n",(int)index,(int)lz_symbols[lz_size].backref);
				RansEncPutSymbol(&rans, &outPointer, esyms_backref + lz_symbols[lz_size].backref);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
			//printf("%d\n",(int)index);
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
		uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_symbols[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		uint8_t* trailing = outPointer;
		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
		printf("lz table size: %d bytes\n",(int)(trailing - outPointer));
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

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		colour_encode_combiner_slow(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner_slow(
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

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01100111;
	}
	else{
		*(--outPointer) = 0b01100110;
	}
}

void colour_optimiser_take6_lz_old(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed,
	bool debug
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

	printf("setting initial predictor layout\n");

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

	printf("testing %d alternate predictors",(int)available_predictors);

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
			filter_collection,
			debug
		);
	}

	printf("\nperforming %d refinement passes\n",(int)speed/2);
	for(size_t i=0;i<speed/2;i++){
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
/*
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
*/
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
	printf("Testing LZ\n");
	bool LZ_used = false;
	lz_triple* lz_data;
	size_t lz_size;
	float* estimate = new float[width*height];

	double* costTables[contextNumber];

	for(size_t i=0;i<contextNumber;i++){
		costTables[i] = entropyLookup(statistics[i]);
	}

	for(size_t i=0;i<width*height;i++){
		size_t tile_index = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		estimate[i] =
			costTables[entropy_image[tile_index*3]][filtered_bytes[i*3]]
			+ costTables[entropy_image[tile_index*3+1]][filtered_bytes[i*3+1]]
			+ costTables[entropy_image[tile_index*3+2]][filtered_bytes[i*3+2]];
	}

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}


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
	lz_size = 0;
	delete[] lz_data;
	lz_triple_c* lz_symbols;
	if(synth > 0.25){
		for(size_t i=0;i<predictorCount;i++){
			delete[] filter_collection[i];
		}
		delete[] filter_collection;
		printf("Trying LZ\n");
		size_t lzlimit;
		if(speed > 18){
			lzlimit = 4 << 18;
		}
		else{
			lzlimit = 4 << speed;
		}
		lz_data = lz_dist_complete(
			in_bytes,
			estimate,
			width,
			height,
			lz_size,
			16,
			lzlimit
		);
/*
		size_t lavasum = 0;
		for(size_t i=1;i<lz_size;i++){
			lavasum += lz_data[i].val_future;
			lavasum += lz_data[i].val_matchlen + 1;
			//printf("val %d %d\n",(int)lz_data[i].val_future,(int)lz_data[i].val_matchlen);
		}
		lavasum += lz_data[0].val_future;
		printf("lz size: %d %d %d\n",(int)lz_size,(int)(width*height),(int)lavasum);*/

		printf("lz size: %d\n",(int)lz_size);
		size_t old_lz_size = lz_size;
		lz_symbols = lz_pruner(
			estimate,
			width,
			lz_data,
			lz_size
		);
		if(lz_size && old_lz_size/lz_size < 10){
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
		}
		if(lz_size > 10){
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

			if(speed > 9){
				double* costTables[contextNumber];

				for(size_t i=0;i<contextNumber;i++){
					costTables[i] = entropyLookup(statistics[i]);
				}

				for(size_t i=0;i<width*height;i++){
					size_t tile_index = tileIndexFromPixel(
						i,
						width,
						entropyWidth,
						entropyWidth_block,
						entropyHeight_block
					);
					estimate[i] =
						costTables[entropy_image[tile_index*3]][filtered_bytes[i*3]]
						+ costTables[entropy_image[tile_index*3+1]][filtered_bytes[i*3+1]]
						+ costTables[entropy_image[tile_index*3+2]][filtered_bytes[i*3+2]];
				}

				for(size_t i=0;i<contextNumber;i++){
					delete[] costTables[i];
				}
/*
				delete[] lz_data;
				lz_data = lz_dist_complete(
					in_bytes,
					estimate,
					width,
					height,
					lz_size,
					16,
					lzlimit
				);
*/
				printf("Doing introspective LZ matching (slow). Only enabled at speed > 9\n");
				lz_dist_selfAware(
					in_bytes,
					estimate,
					width,
					height,
					lz_data,
					lz_symbols,
					lz_size,
					16,
					lzlimit
				);
				printf("lz size: %d\n",(int)lz_size);
				delete[] lz_symbols;
				lz_symbols = lz_pruner(
					estimate,
					width,
					lz_data,
					lz_size
				);
				printf("lz size: %d\n",(int)lz_size);
				if(speed > 12){
					printf("Doing second introspective LZ matching (slow). Only enabled at speed > 12\n");
					lz_dist_selfAware(
						in_bytes,
						estimate,
						width,
						height,
						lz_data,
						lz_symbols,
						lz_size,
						16,
						lzlimit
					);
					printf("lz size: %d\n",(int)lz_size);
					delete[] lz_symbols;
					lz_symbols = lz_pruner(
						estimate,
						width,
						lz_data,
						lz_size
					);
					printf("lz size: %d\n",(int)lz_size);
				}


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
		}
		else{
			delete[] lz_data;
		}
		delete[] estimate;
	}
	else{
		delete[] estimate;
		printf("performing %d refinement passes\n",(int)(speed - speed/2));
		for(size_t i=0;i<speed - speed/2;i++){
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
		for(size_t i=0;i<predictorCount;i++){
			delete[] filter_collection[i];
		}
		delete[] filter_collection;
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
		SymbolStats stats_backref;
		SymbolStats stats_matchlen;
		SymbolStats stats_future;
		for(size_t i=0;i<256;i++){
			stats_backref.freqs[i] = 0;
			stats_matchlen.freqs[i] = 0;
			stats_future.freqs[i] = 0;
		}
		//printf("lz data %d\n",(int)lz_data[0].future);
		for(size_t i=1;i<lz_size;i++){
			stats_backref.freqs[lz_symbols[i].backref]++;
			stats_matchlen.freqs[lz_symbols[i].matchlen]++;
			stats_future.freqs[lz_symbols[i].future]++;
		}
		stats_future.freqs[lz_symbols[0].future]++;

		BitWriter lz_tableEncode;
		SymbolStats lz_table_backref = encode_freqTable(stats_backref, lz_tableEncode, 200);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 40);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, 40);
		lz_tableEncode.conclude();

		RansEncSymbol esyms_backref[256];
		RansEncSymbol esyms_matchlen[256];
		RansEncSymbol esyms_future[256];
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms_backref[i], lz_table_backref.cum_freqs[i], lz_table_backref.freqs[i], 16);
			RansEncSymbolInit(&esyms_matchlen[i],  lz_table_matchlen.cum_freqs[i],  lz_table_matchlen.freqs[i],  16);
			RansEncSymbolInit(&esyms_future[i],    lz_table_future.cum_freqs[i],    lz_table_future.freqs[i],    16);
		}

		double backref_cost = 0;
		double matchlen_cost = 0;
		double future_cost = 0;

		for(size_t i=0;i<40;i++){
			uint8_t extrabits = extrabits_from_prefix(i);
			if(lz_table_matchlen.freqs[i]){
				matchlen_cost += stats_matchlen.freqs[i] * (extrabits - std::log2((double)lz_table_matchlen.freqs[i]) + 16);
			}
			if(lz_table_future.freqs[i]){
				future_cost += stats_future.freqs[i] * (extrabits - std::log2((double)lz_table_future.freqs[i]) + 16);
			}
			if(lz_table_backref.freqs[160+i]){
				backref_cost += stats_backref.freqs[160+i] * (extrabits - std::log2((double)lz_table_backref.freqs[160+i]) + 16);
			}
		}
		for(size_t i=0;i<120;i++){
			if(lz_table_backref.freqs[i]){
				backref_cost += stats_backref.freqs[i] * (-std::log2((double)lz_table_backref.freqs[i]) + 16);
			}
		}
		for(size_t i=0;i<20;i++){
			uint8_t extrabits = extrabits_from_prefix(i);
			if(lz_table_backref.freqs[120+i]){
				backref_cost += stats_backref.freqs[i] * (extrabits - std::log2((double)lz_table_backref.freqs[120+i]) + 16 + 5);
			}
			if(lz_table_backref.freqs[140+i]){
				backref_cost += stats_backref.freqs[i] * (extrabits - std::log2((double)lz_table_backref.freqs[140+i]) + 16);
			}
		}
		printf("LZ: backref %f bytes, matchlen: %f bytes, offset: %f bytes\n",backref_cost/8,matchlen_cost/8,future_cost/8);

		RansEncSymbol binary_zero;
		RansEncSymbol binary_one;
		RansEncSymbolInit(&binary_zero, 0, (1 << 15), 16);
		RansEncSymbolInit(&binary_one,  (1 << 15), (1 << 15), 16);

		uint32_t next_match = lz_data[--lz_size].val_future;
		for(size_t index=width*height;index--;){
			//printf("%d\n",(int)index);
			if(next_match == 0){
				index -= lz_data[lz_size].val_matchlen;

				uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].future_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("a %d\n",(int)index);
				RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);

				loose_bytes = lz_symbols[lz_size].matchlen_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].matchlen_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("b %d %d\n",(int)index,(int)lz_symbols[lz_size].matchlen);
				RansEncPutSymbol(&rans, &outPointer, esyms_matchlen + lz_symbols[lz_size].matchlen);

				loose_bytes = lz_symbols[lz_size].backref_bits >> 24;
				//printf("loose %d\n",(int)loose_bytes);
				for(size_t shift = 0;shift < loose_bytes;shift++){
					//printf("shift %d\n",(int)shift);
					if((lz_symbols[lz_size].backref_bits >> shift) & 1){
						RansEncPutSymbol(&rans, &outPointer, &binary_one);
					}
					else{
						RansEncPutSymbol(&rans, &outPointer, &binary_zero);
					}
				}
				//printf("c %d %d\n",(int)index,(int)lz_symbols[lz_size].backref);
				RansEncPutSymbol(&rans, &outPointer, esyms_backref + lz_symbols[lz_size].backref);

				next_match = lz_data[--lz_size].val_future;
				continue;
			}
			else{
				next_match--;
			}
			//printf("%d\n",(int)index);
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
		uint8_t loose_bytes = lz_symbols[lz_size].future_bits >> 24;
		//printf("loose %d\n",(int)loose_bytes);
		for(size_t shift = 0;shift < loose_bytes;shift++){
			//printf("shift %d\n",(int)shift);
			if((lz_symbols[lz_size].future_bits >> shift) & 1){
				RansEncPutSymbol(&rans, &outPointer, &binary_one);
			}
			else{
				RansEncPutSymbol(&rans, &outPointer, &binary_zero);
			}
		}
		RansEncPutSymbol(&rans, &outPointer, esyms_future + lz_symbols[lz_size].future);
		RansEncFlush(&rans, &outPointer);

		uint8_t* trailing = outPointer;
		for(size_t i=lz_tableEncode.length;i--;){
			*(--outPointer) = lz_tableEncode.buffer[i];
		}
		printf("lz table size: %d bytes\n",(int)(trailing - outPointer));
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

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		colour_encode_combiner_slow(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		colour_encode_combiner_slow(
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

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01100111;
	}
	else{
		*(--outPointer) = 0b01100110;
	}
}

#endif //COLOUR_OPTIMISER
