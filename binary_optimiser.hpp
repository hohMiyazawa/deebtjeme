#ifndef BINARY_OPTIMISER_HEADER
#define BINARY_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "prefix_coding.hpp"
#include "lz_optimiser.hpp"
#include "simple_encoders.hpp"


void encode_grey_binary(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer,uint8_t col1,uint8_t col2){
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

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode, 2);
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

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	*(--outPointer) = col2;
	*(--outPointer) = col1;
	printf("cols %d,%d\n",(int)col1,(int)col2);
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 1;//width

	*(--outPointer) = 0b00010110;
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
		16,16,5
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
		SymbolStats lz_table_backref = encode_freqTable(stats_backref, lz_tableEncode, 212);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 44);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, 44);
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

void encode_grey_binary_entropy_lz2(uint8_t* in_bytes, uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer,uint8_t col1,uint8_t col2, size_t speed){
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
		16,16,5
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
	lz_data = lz_dist_modern_grey(
		in_bytes,
		estimate,
		width,
		height,
		backref_cost_table,
		matchlen_cost_table,
		future_cost_table,
		lz_size,
		4 << speed
	);
	printf("lz size: %d\n",(int)lz_size);
	delete[] backref_cost_table;
	delete[] matchlen_cost_table;
	delete[] future_cost_table;

	lz_triple_c* lz_symbols = lz_pruner(
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
	lz_data = lz_dist_modern_grey(
		in_bytes,
		estimate,
		width,
		height,
		backref_cost_table,
		matchlen_cost_table,
		future_cost_table,
		lz_size,
		16 << speed
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
	delete[] lz_symbols;
	lz_symbols = lz_pruner(
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
		SymbolStats lz_table_backref = encode_freqTable(stats_backref, lz_tableEncode, 212);
		SymbolStats lz_table_matchlen = encode_freqTable(stats_matchlen, lz_tableEncode, 44);
		SymbolStats lz_table_future = encode_freqTable(stats_future, lz_tableEncode, 44);
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

#endif //BINARY_OPTIMISER
