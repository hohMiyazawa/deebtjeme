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
	SymbolStats table = encode_freqTable(stats,tableEncode, colour_count);
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

	*(--outPointer) = 0b01010010;
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

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = 0b01010110;
}

void indexed_encode_speed2(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
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
		table[context] = encode_freqTable(statistics[context], tableEncode, colour_count);
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

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = 0b01010110;
}

void indexed_encode_speed3(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
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

	lz_data = lz_dist_fast_grey(
		in_bytes,
		width,
		height,
		lz_size,
		16
	);
	lz_triple_c* lz_symbols = lz_translator(
		lz_data,
		width,
		lz_size
	);

	size_t matchlen_sum = 0;
	for(size_t i=1;i<lz_size;i++){
		matchlen_sum += 1 + lz_data[i].val_matchlen;
	}
	double synth = (double)matchlen_sum/(width*height);
	printf("synth %f\n",synth);

	if(synth > 0.5){
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
		table[context] = encode_freqTable(statistics[context], tableEncode, colour_count);
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

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01010111;
	}
	else{
		*(--outPointer) = 0b01010110;
	}
}

void indexed_encode_speed4(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
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
		16,
		(16 << speed)
	);
	lz_triple_c* lz_symbols = lz_pruner(
		estimate,
		width,
		lz_data,
		lz_size
	);
	if(speed > 5){
		delete[] lz_symbols;
		lz_symbols = lz_pruner(
			estimate,
			width,
			lz_data,
			lz_size
		);
	}
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
		table[context] = encode_freqTable(statistics[context], tableEncode, colour_count);
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

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	if(LZ_used){
		delete[] lz_data;
		delete[] lz_symbols;
		*(--outPointer) = 0b01010111;
	}
	else{
		*(--outPointer) = 0b01010110;
	}
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
	else if(speed == 1){
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
	else if(speed == 2){
		indexed_encode_speed2(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer,
			speed
		);
	}
	else{
		indexed_encode_speed4(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer,
			speed
		);
	}
}

#endif //INDEX_ENCODER_HEADER
