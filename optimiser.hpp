#ifndef OPTIMISER_HEADER
#define OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_optimiser.hpp"
#include "predictor_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "simple_encoders.hpp"
#include "lz_optimiser.hpp"

void encode_combiner(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	size_t safety_margin = width*height * (log2_plus(range - 1) + 1) + 2048;

	uint8_t alternates = 1;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing_end[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing_end[i] = miniBuffer[i] + safety_margin;
		trailing[i] = trailing_end[i];
	}
	encode_entropy(in_bytes,range,width,height,trailing[0]);
/*
	encode_ffv1(in_bytes,range,width,height,trailing[1]);
	encode_left(in_bytes,range,width,height,trailing[2]);
	encode_top(in_bytes,range,width,height,trailing[3]);*/
	//research_optimiser_entropyOnly(in_bytes,range,width,height,miniBuffer[2],1);
	for(size_t i=0;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		printf("type %d: %d\n",(int)i,(int)diff);
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

void encode_grey_binary_entropy(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer,uint8_t col1,uint8_t col2, size_t speed){
	uint8_t* filtered_bytes = new uint8_t[width*height];

	for(size_t i=0;i<width*height;i++){
		if(i == 0){
			if(in_bytes[i] == col1){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else if(i < width){
			if(in_bytes[i] == in_bytes[i-1]){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else if(i % width == 0){
			if(in_bytes[i] == in_bytes[i-width]){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else{
			if(in_bytes[i-1] == in_bytes[i-width-1]){
				if(in_bytes[i] == in_bytes[i-width]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
			else if(in_bytes[i-width] == in_bytes[i-width-1]){
				if(in_bytes[i] == in_bytes[i-1]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
			else{
				if(in_bytes[i] == in_bytes[i-1]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
		}
	}

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight,
		16,16,4
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
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

	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, 2);
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		encode_entropy(
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

	printf("contexts: %d\n",(int)contextNumber);
	*(--outPointer) = contextNumber - 1;//number of contexts

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	*(--outPointer) = col2;
	*(--outPointer) = col1;
	printf("cols %d,%d\n",(int)col1,(int)col2);
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;//height
	*(--outPointer) = 1;//width

	*(--outPointer) = 0b00010110;
}

void encode_grey_binary_entropy_lz(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer,uint8_t col1,uint8_t col2, size_t speed){
	uint8_t* filtered_bytes = new uint8_t[width*height];

	for(size_t i=0;i<width*height;i++){
		if(i == 0){
			if(in_bytes[i] == col1){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else if(i < width){
			if(in_bytes[i] == in_bytes[i-1]){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else if(i % width == 0){
			if(in_bytes[i] == in_bytes[i-width]){
				filtered_bytes[i] = 0;
			}
			else{
				filtered_bytes[i] = 1;
			}
		}
		else{
			if(in_bytes[i-1] == in_bytes[i-width-1]){
				if(in_bytes[i] == in_bytes[i-width]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
			else if(in_bytes[i-width] == in_bytes[i-width-1]){
				if(in_bytes[i] == in_bytes[i-1]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
			else{
				if(in_bytes[i] == in_bytes[i-1]){
					filtered_bytes[i] = 0;
				}
				else{
					filtered_bytes[i] = 1;
				}
			}
		}
	}

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight,
		16,16,4
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
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
		estimate[i] = costTables[entropy_image[tile_index]][filtered_bytes[i]];
	}

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}

	printf("Trying LZ\n");
	lz_data = lz_dist_complete_grey(
		in_bytes,
		estimate,
		width,
		height,
		lz_size,
		24,
		(16 << speed)
	);
	lz_triple_c* lz_symbols = lz_pruner(
		estimate,
		width,
		lz_data,
		lz_size
	);
	delete[] estimate;

	if(lz_size > 10){
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
			statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
		}
	}
	else{
		delete[] lz_data;
		delete[] lz_symbols;
	}

	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, 2);
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
			RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
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
			RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
		}
		RansEncFlush(&rans, &outPointer);
	}
	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		encode_entropy(
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

	printf("contexts: %d\n",(int)contextNumber);
	*(--outPointer) = contextNumber - 1;//number of contexts

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	*(--outPointer) = col2;
	*(--outPointer) = col1;
	printf("cols %d,%d\n",(int)col1,(int)col2);
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;//height
	*(--outPointer) = 1;//width

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b00010111;
	}
	else{
		*(--outPointer) = 0b00010110;
	}
}


void optimiser_take0(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight,
		0,0,0//use defaults
	);

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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
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
		predictorCount = add_predictor_maybe(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	uint8_t* trailing = outPointer;
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
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	printf("contexts: %d\n",(int)contextNumber);
	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		encode_entropy(
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


	*(--outPointer) = 0b00000110;
}


void optimiser_take1(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
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
		predictorCount = add_predictor_maybe(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	uint8_t* trailing;

	trailing = outPointer;
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
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		encode_entropy(
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

	*(--outPointer) = 0b00000110;
}

void optimiser_take2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
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
		predictorCount = add_predictor_maybe(
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

	for(size_t i=3;i<7;i++){
		predictorCount = add_predictor_maybe(
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

	for(size_t i=7;i<available_predictors;i++){
		predictorCount = add_predictor_maybe(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	uint8_t* trailing;

	trailing = outPointer;
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
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		encode_entropy(
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

	*(--outPointer) = 0b00000110;
}

void optimiser_take3(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t** filter_collection = new uint8_t*[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);
	filter_collection[0] = filter_all_ffv1(in_bytes, range, width, height);

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
		predictorCount = add_predictor_maybe_prefiltered(
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass_prefiltered(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	encode_entropy(
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
		encode_entropy(
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


	*(--outPointer) = 0b00000110;
}

void optimiser_take4(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t** filter_collection = new uint8_t*[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);
	filter_collection[0] = filter_all_ffv1(in_bytes, range, width, height);

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
		predictorCount = add_predictor_maybe_prefiltered(
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass_prefiltered(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	encode_combiner(
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
		encode_combiner(
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

	*(--outPointer) = 0b00000110;
}

void optimiser_take5(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t** filter_collection = new uint8_t*[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);
	filter_collection[0] = filter_all_ffv1(in_bytes, range, width, height);

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
		predictorCount = add_predictor_maybe_prefiltered(
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass_prefiltered(
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
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	encode_combiner(
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
		encode_combiner(
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


	*(--outPointer) = 0b00000110;
}

void optimiser_take5_lz(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
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
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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

	uint16_t* predictors = new uint16_t[256];
	uint8_t** filter_collection = new uint8_t*[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);
	filter_collection[0] = filter_all_ffv1(in_bytes, range, width, height);

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
		predictorCount = add_predictor_maybe_prefiltered(
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass_prefiltered(
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

//one pass for stat tables
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
		estimate[i] = costTables[entropy_image[tile_index]][filtered_bytes[i]];
	}

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}

	printf("Trying LZ\n");
	lz_data = lz_dist_complete_grey(
		in_bytes,
		estimate,
		width,
		height,
		lz_size,
		16,
		(16 << speed)
	);
	printf("lz size: %d\n",(int)lz_size);
	lz_triple_c* lz_symbols = lz_pruner(
		estimate,
		width,
		lz_data,
		lz_size
	);
	printf("lz size: %d\n",(int)lz_size);

	if(lz_size > 20){
		LZ_used = true;
		if(speed > 8){
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
		}
		if(speed > 10){
			delete[] lz_symbols;
			lz_symbols = lz_pruner(
				estimate,
				width,
				lz_data,
				lz_size
			);
		}
		delete[] estimate;

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
			statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
		}
	}
	else{
		delete[] estimate;
		delete[] lz_data;
		delete[] lz_symbols;
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
		/*printf(
			"inv pref %d %d %d %d\n",
			(int)(inverse_prefix((width + 1)/2)*2 + 2),
			(int)(inverse_prefix(height) + 1),
			(int)(inverse_prefix(width*height) + 1),
			(int)(inverse_prefix(width*height) + 1)
		);*/

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
			RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
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
			RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
		}
		RansEncFlush(&rans, &outPointer);
	}
	delete[] filtered_bytes;

	printf("ransenc done\n");

	for(size_t i=0;i<predictorCount;i++){
		delete[] filter_collection[i];
	}
	delete[] filter_collection;

	uint8_t* trailing;

	trailing = outPointer;
	encode_combiner(
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
		encode_combiner(
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
		*(--outPointer) = 0b00000111;
	}
	else{
		*(--outPointer) = 0b00000110;
	}
}
#endif //OPTIMISER
