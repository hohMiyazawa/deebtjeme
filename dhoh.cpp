#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "file_io.hpp"
#include "2dutils.hpp"
#include "varint.hpp"
#include "lode_io.hpp"
#include "symbolstats.hpp"
#include "table_decode.hpp"
#include "rans_byte.h"
#include "bitreader.hpp"
#include "unfilters.hpp"
#include "delta_colour.hpp"
#include "prefix_coding.hpp"
#include "colour_unfilters.hpp"

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

void sanity_check(
	bool TILES,
	bool PROGRESSIVE,
	bool HAS_COLOUR,
	bool INDEX_TRANSFORM,
	bool COLOUR_TRANSFORM,
	bool PREDICTION_MAP,
	bool ENTROPY_MAP,
	bool LZ
){
	if(TILES){
		panic("TILES bit set!\n");
	}
	if(PROGRESSIVE){
		panic("PROGRESSIVE bit set!\n");
	}
	if(!HAS_COLOUR && COLOUR_TRANSFORM){
		panic("Can not use colour transform for greyscale!\n");
	}
	if(INDEX_TRANSFORM && COLOUR_TRANSFORM){
		panic("Can not use colour transform and index transform at the same time!\n");
	}
	if(
		!ENTROPY_MAP
		&& (
			COLOUR_TRANSFORM
			|| PREDICTION_MAP
			|| LZ
		)
	){
		panic("Nonsensical features without entropy coding!\n");
	}
}

uint8_t* readImage(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t compressionMode = *(fileIndex++);

	bool TILES            = (compressionMode & 0b10000000) >> 7;
	bool PROGRESSIVE      = (compressionMode & 0b01000000) >> 6;
	bool HAS_COLOUR       = (compressionMode & 0b00100000) >> 5;
	bool INDEX_TRANSFORM  = (compressionMode & 0b00010000) >> 4;
	bool COLOUR_TRANSFORM = (compressionMode & 0b00001000) >> 3;
	bool PREDICTION_MAP   = (compressionMode & 0b00000100) >> 2;
	bool ENTROPY_MAP      = (compressionMode & 0b00000010) >> 1;
	bool LZ               = (compressionMode & 0b00000001) >> 0;

	sanity_check(
		TILES,
		PROGRESSIVE,
		HAS_COLOUR,
		INDEX_TRANSFORM,
		COLOUR_TRANSFORM,
		PREDICTION_MAP,
		ENTROPY_MAP,
		LZ
	);
	uint8_t* indexImage;
	uint8_t* indexIndexImage;
	uint32_t indexWidth;
	uint32_t indexHeight;
	uint32_t indexIndexWidth = 1;
	uint32_t indexIndexHeight = 1;
	uint32_t indexIndexWidth_block = width;
	uint32_t indexIndexHeight_block = height;
	uint8_t* indexLengths;
	if(INDEX_TRANSFORM){
		indexWidth = *(fileIndex++) + 1;
		indexHeight = *(fileIndex++) + 1;
		indexLengths = new uint8_t[height];
		indexImage = readImage(fileIndex, 256, indexWidth, indexHeight);
		//printf("index %d,%d,%d %d,%d,%d\n",(int)indexImage[0],(int)indexImage[1],(int)indexImage[2],(int)indexImage[3],(int)indexImage[4],(int)indexImage[5]);
		if(indexHeight > 1){
			indexIndexWidth = readVarint(fileIndex) + 1;
			indexIndexHeight = readVarint(fileIndex) + 1;
			indexIndexWidth_block  = (width + indexIndexWidth - 1)/indexIndexWidth;
			indexIndexHeight_block = (height + indexIndexHeight - 1)/indexIndexHeight;
			indexIndexImage = readImage(fileIndex, indexHeight,indexIndexWidth,indexIndexHeight);
			
			for(size_t y=0;y<indexHeight;y++){
				indexLengths[y] = indexWidth - 1;
				for(size_t i=1;i<indexWidth;i++){
					if(
						indexImage[i*3] == indexImage[(i-1)*3]
						&& indexImage[i*3+1] == indexImage[(i-1)*3+1]
						&& indexImage[i*3+2] == indexImage[(i-1)*3+2]
					){
						indexLengths[y] = i - 1;
						break;
					}
				}
			}
		}
		else{
			indexIndexImage = new uint8_t[3];
			indexIndexImage[0] = 0;
			indexIndexImage[1] = 0;
			indexIndexImage[2] = 0;
			indexLengths[0] = indexWidth - 1;
		}
	}


	uint32_t colourWidth = 1;
	uint32_t colourHeight = 1;
	uint32_t colourWidth_block = width;
	uint32_t colourHeight_block = height;
	uint8_t* colourImage;
	if(COLOUR_TRANSFORM){
		uint8_t* trailing = fileIndex;
		colourWidth = readVarint(fileIndex) + 1;
		colourHeight = readVarint(fileIndex) + 1;
		colourWidth_block  = (width + colourWidth - 1)/colourWidth;
		colourHeight_block = (height + colourHeight - 1)/colourHeight;
		printf("---\n");
		printf("  colour transform image %d x %d\n",(int)colourWidth,(int)colourHeight);
		colourImage = readImage(fileIndex, 256,colourWidth,colourHeight);
		printf("---colour transform image size: %d bytes\n",(int)(fileIndex - trailing));
	}

	uint16_t predictors[256];
	uint8_t predictorCount = 0;
	uint16_t* predictorImage;
	uint32_t predictorWidth = 1;
	uint32_t predictorHeight = 1;
	uint32_t predictorWidth_block = width;
	uint32_t predictorHeight_block = height;
	if(PREDICTION_MAP){
		predictorCount = (*(fileIndex++)) + 1;
		for(size_t i=0;i<predictorCount;i++){
			uint16_t value = ((*(fileIndex++)) << 8);
			value += *(fileIndex++);
			predictors[i] = value;
		}
		printf("  %d predictors\n",(int)predictorCount);
		if(predictorCount > 1){
			uint8_t* trailing = fileIndex;
			predictorWidth = readVarint(fileIndex) + 1;
			predictorHeight = readVarint(fileIndex) + 1;
			predictorWidth_block  = (width + predictorWidth - 1)/predictorWidth;
			predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;
			printf("---\n");
			printf("  predictor image %d x %d\n",(int)predictorWidth,(int)predictorHeight);
			uint8_t* predictorImage_data = readImage(fileIndex,predictorCount,predictorWidth,predictorHeight);
			printf("---predictor image size: %d bytes\n",(int)(fileIndex - trailing));
			predictorImage = new uint16_t[predictorWidth*predictorHeight*3];
			for(size_t i=0;i<predictorWidth*predictorHeight;i++){
				predictorImage[i*3 + 0] = predictors[predictorImage_data[i*3 + 0]];
				predictorImage[i*3 + 1] = predictors[predictorImage_data[i*3 + 1]];
				predictorImage[i*3 + 2] = predictors[predictorImage_data[i*3 + 2]];
			}
			delete[] predictorImage_data;
		}
		else{
			predictorImage = new uint16_t[3];
			predictorImage[0] = predictors[0];
			predictorImage[1] = predictors[0];
			predictorImage[2] = predictors[0];
		}
	}

	uint8_t entropyContexts = 0;
	uint8_t* entropyImage;
	uint32_t entropyWidth;
	uint32_t entropyHeight;
	uint32_t entropyWidth_block = width;
	uint32_t entropyHeight_block = height;
	if(ENTROPY_MAP){
		entropyContexts = *(fileIndex++) + 1;
		printf("  %d entropy contexts\n",(int)entropyContexts);
	}

	uint8_t* trailing = fileIndex;

	SymbolStats tables[entropyContexts];
	if(entropyContexts){
		BitReader reader(&fileIndex);
		for(size_t i=0;i<entropyContexts;i++){
			tables[i] = decode_freqTable(reader,range);
		}
		printf("  entropy table size: %d bytes\n",(int)(fileIndex - trailing));
	}

	if(entropyContexts > 1){
		uint8_t* trailing = fileIndex;
		entropyWidth = readVarint(fileIndex) + 1;
		entropyHeight = readVarint(fileIndex) + 1;
		entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
		entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
		printf("---\n");
		printf("  entropy image %d x %d\n",(int)entropyWidth,(int)entropyHeight);
		entropyImage = readImage(fileIndex,entropyContexts,entropyWidth,entropyHeight);
		printf("---entropy image size: %d bytes\n",(int)(fileIndex - trailing));
	}
	else{
		entropyImage = new uint8_t[3];
		entropyImage[0] = 0;
		entropyImage[1] = 0;
		entropyImage[2] = 0;
	}

	size_t lz_next = width*height;
	uint8_t lz_bit_buffer = 0;
	uint8_t lz_bit_buffer_index = 0;

	RansDecSymbol dsyms[entropyContexts][256];

	SymbolStats backref_x_table;
	SymbolStats backref_y_table;
	SymbolStats matchlen_table;
	SymbolStats futureref_table;
	RansDecSymbol dbx[256];
	RansDecSymbol dby[256];
	RansDecSymbol dml[256];
	RansDecSymbol dfr[256];

	uint8_t max_x_table_prefix = inverse_prefix((width + 1)/2);
	uint8_t max_y_table_prefix = inverse_prefix(height);
	uint8_t max_ml_table_prefix = inverse_prefix(width*height);
	uint8_t max_fr_table_prefix = inverse_prefix(width*height);

	if(LZ){
		BitReader reader(&fileIndex);
		backref_x_table = decode_freqTable(reader, max_x_table_prefix*2 + 1);
		backref_y_table = decode_freqTable(reader, max_y_table_prefix + 1);
		matchlen_table  = decode_freqTable(reader, max_ml_table_prefix + 1);
		futureref_table = decode_freqTable(reader, max_fr_table_prefix + 1);
	}
	RansState rans;
	if(ENTROPY_MAP){
		RansDecInit(&rans, &fileIndex);
		for(size_t cont = 0;cont < entropyContexts;cont++){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
			}
		}
		if(LZ){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dbx[i], backref_x_table.cum_freqs[i], backref_x_table.freqs[i]);
				RansDecSymbolInit(&dby[i], backref_y_table.cum_freqs[i], backref_y_table.freqs[i]);
				RansDecSymbolInit(&dml[i], matchlen_table.cum_freqs[i], matchlen_table.freqs[i]);
				RansDecSymbolInit(&dfr[i], futureref_table.cum_freqs[i], futureref_table.freqs[i]);
			}
			lz_next = prefix_to_val(read_prefixcode(&rans, dfr, futureref_table, &fileIndex), lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
		}
	}

	uint16_t rcache[width];
	uint16_t bcache[width];
	uint16_t rcache_L = 0;
	uint16_t bcache_L = 0;

	size_t lz_next_cache = lz_next;
	size_t lz_bx_cache = 0;
	size_t lz_by_cache = 0;
	size_t lz_ml_cache = 0;

	uint8_t* image = new uint8_t[width*height*3];
	printf("decoding pixels\n");
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint8_t backref_x_prefix = read_prefixcode(&rans, dbx, backref_x_table, &fileIndex);
			size_t backref_x;
			if(backref_x_prefix == 0){
				backref_x = lz_bx_cache;
			}
			else{
				if(backref_x_prefix <= max_x_table_prefix){
					backref_x = prefix_to_val(backref_x_prefix - 1, lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
				}
				else{
					backref_x = width - prefix_to_val(max_x_table_prefix*2 - backref_x_prefix, lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
				}
				lz_bx_cache = backref_x;
			}
			uint8_t backref_y_prefix = read_prefixcode(&rans, dby, backref_y_table, &fileIndex);
			size_t backref_y;
			if(backref_y_prefix == 0){
				backref_y = lz_by_cache;
			}
			else{
				backref_y = prefix_to_val(backref_y_prefix - 1, lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
				lz_by_cache = backref_y;
			}
			size_t backref = backref_y * width + backref_x + 1;
			uint8_t matchlen_prefix = read_prefixcode(&rans, dml, matchlen_table, &fileIndex);
			size_t matchlen;
			if(matchlen_prefix == 0){
				matchlen = lz_ml_cache;
			}
			else{
				matchlen = prefix_to_val(matchlen_prefix - 1, lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
				lz_ml_cache = matchlen;
			}
			matchlen += 1;
			uint8_t lz_next_prefix = read_prefixcode(&rans, dfr, futureref_table, &fileIndex);
			if(lz_next_prefix == 0){
				lz_next = lz_next_cache;
			}
			else{
				lz_next = prefix_to_val(lz_next_prefix - 1, lz_bit_buffer, lz_bit_buffer_index, &fileIndex);
				lz_next_cache = lz_next;
			}
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
				if(COLOUR_TRANSFORM && PREDICTION_MAP){
					size_t colourIndex = tileIndexFromPixel(
						i + t,
						width,
						colourWidth,
						colourWidth_block,
						colourHeight_block
					);
					uint16_t rg_delta = delta(colourImage[colourIndex*3],image[(i+t)*3]);
					rcache[(i + t - 1) % width] = rcache_L;
					rcache_L = image[(i+t)*3 + 1] + range - rg_delta;
					uint16_t bg_delta = delta(colourImage[colourIndex*3 + 1],image[(i+t)*3]);
					uint16_t br_delta = delta(colourImage[colourIndex*3 + 2],image[(i+t)*3 + 1]);
					bcache[(i + t - 1) % width] = bcache_L;
					bcache_L = image[(i+t)*3 + 2] + 2*range - bg_delta - br_delta;
				}
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		size_t entropyIndex;
		if(ENTROPY_MAP){
			entropyIndex = tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			);
			uint32_t cumFreq = RansDecGet(&rans, 16);
			uint8_t s;
			for(size_t j=0;j<256;j++){
				if(tables[entropyImage[entropyIndex*3]].cum_freqs[j + 1] > cumFreq){
					s = j;
					break;
				}
			}
			RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[entropyIndex*3]][s], 16);
			image[i*3] = s;
		}
		else{
			image[i*3] = *(fileIndex++);
		}

		size_t localRange = range;
		size_t indexIndexIndex;
		uint8_t index_to_use;
		if(INDEX_TRANSFORM){
			indexIndexIndex = tileIndexFromPixel(
				i,
				width,
				indexIndexWidth,
				indexIndexWidth_block,
				indexIndexHeight_block
			);
			index_to_use = indexIndexImage[indexIndexIndex*3];
			localRange = indexLengths[index_to_use] + 1;
		}

		size_t predictorIndex;
		if(PREDICTION_MAP){
			if(i == 0){
				//nothing
			}
			else if(i < width){
				image[i*3] = add_mod(image[i*3],image[(i-1)*3],localRange);
			}
			else if(i % width == 0){
				image[i*3] = add_mod(image[i*3],image[(i-width)*3],localRange);
			}
			else{
				predictorIndex = tileIndexFromPixel(
					i,
					width,
					predictorWidth,
					predictorWidth_block,
					predictorHeight_block
				);
				uint16_t predictor = predictorImage[predictorIndex*3];
				int a =       (predictor & 0b1111000000000000) >> 12;
				int b =       (predictor & 0b0000111100000000) >> 8;
				int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
				int d =       (predictor & 0b0000000000001111);

				uint8_t L = image[(i-1)*3];
				uint8_t T = image[(i-width)*3];
				uint8_t TL = image[(i-width-1)*3];

				if(predictor == 0){
					image[i*3] = add_mod(
						image[i*3],
						ffv1(
							L,
							T,
							TL
						),
						localRange
					);
				}
				else{
					uint8_t TR = image[(i-width+1)*3];
					uint8_t sum = a + b + c + d;
					uint8_t halfsum = sum >> 1;

					image[i*3] = add_mod(
						image[i*3],
						clamp(
							(
								a*L + b*T + c*TL + d*TR + halfsum
							)/sum,
							localRange
						),
						localRange
					);
				}
			}
		}

		if(HAS_COLOUR == 0){
			image[i*3+1] = image[i*3];
			image[i*3+2] = image[i*3];
		}
		else{
			if(ENTROPY_MAP){
				uint32_t cumFreq = RansDecGet(&rans, 16);
				uint8_t s;
				for(size_t j=0;j<256;j++){
					if(tables[entropyImage[entropyIndex*3+1]].cum_freqs[j + 1] > cumFreq){
						s = j;
						break;
					}
				}
				RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[entropyIndex*3+1]][s], 16);
				image[i*3+1] = s;
				cumFreq = RansDecGet(&rans, 16);
				for(size_t j=0;j<256;j++){
					if(tables[entropyImage[entropyIndex*3+2]].cum_freqs[j + 1] > cumFreq){
						s = j;
						break;
					}
				}
				RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[entropyIndex*3+2]][s], 16);
				image[i*3+2] = s;
			}
			else{
				image[i*3+1] = *(fileIndex++);
				image[i*3+2] = *(fileIndex++);
			}
			if(COLOUR_TRANSFORM == 0){
				if(PREDICTION_MAP){
					if(i == 0){
						//nothing
					}
					else if(i < width){
						image[i*3+1] = add_mod(image[i*3+1],image[(i-1)*3+1],range);
						image[i*3+2] = add_mod(image[i*3+2],image[(i-1)*3+2],range);
					}
					else if(i % width == 0){
						image[i*3+1] = add_mod(image[i*3+1],image[(i-width)*3+1],range);
						image[i*3+2] = add_mod(image[i*3+2],image[(i-width)*3+2],range);
					}
					else{
						uint16_t predictor = predictorImage[predictorIndex*3+1];
						int a = (predictor & 0b1111000000000000) >> 12;
						int b = (predictor & 0b0000111100000000) >> 8;
						int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
						int d = (predictor & 0b0000000000001111);

						uint8_t sum = a + b + c + d;
						uint8_t halfsum = sum >> 1;
						if(predictor == 0){
							image[i*3+1] = add_mod(
								image[i*3+1],
								ffv1(
									image[(i-1)*3+1],
									image[(i-width)*3+1],
									image[(i-width-1)*3+1]
								),
								range
							);
						}
						else{
							image[i*3+1] = add_mod(
								image[i*3+1],
								clamp(
									(
										a*image[(i-1)*3+1] + b*image[(i-width)*3+1] + c*image[(i-width-1)*3+1] + d*image[(i-width+1)*3+1] + halfsum
									)/sum,
									range
								),
								range
							);
						}


						predictor = predictorImage[predictorIndex*3+2];
						a = (predictor & 0b1111000000000000) >> 12;
						b = (predictor & 0b0000111100000000) >> 8;
						c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
						d = (predictor & 0b0000000000001111);

						sum = a + b + c + d;
						halfsum = sum >> 1;
						if(predictor == 0){
							image[i*3+2] = add_mod(
								image[i*3+2],
								ffv1(
									image[(i-1)*3+2],
									image[(i-width)*3+2],
									image[(i-width-1)*3+2]
								),
								range
							);
						}
						else{
							image[i*3+2] = add_mod(
								image[i*3+2],
								clamp(
									(
										a*image[(i-1)*3+2] + b*image[(i-width)*3+2] + c*image[(i-width-1)*3+2] + d*image[(i-width+1)*3+2] + halfsum
									)/sum,
									range
								),
								range
							);
						}
					}
				}
			}
			else{
				size_t colourIndex = tileIndexFromPixel(
					i,
					width,
					colourWidth,
					colourWidth_block,
					colourHeight_block
				);
				uint8_t rg = colourImage[colourIndex*3];
				uint8_t bg = colourImage[colourIndex*3+1];
				uint8_t br = colourImage[colourIndex*3+2];
				uint8_t rg_delta = delta(rg,image[i*3]);
				uint8_t bg_delta = delta(bg,image[i*3]);
				if(PREDICTION_MAP){
					if(i == 0){
						image[i*3+1] = add_mod(image[i*3+1],rg_delta,range);
						uint8_t br_delta = delta(br,image[i*3+1]);
						image[i*3+2] = add_mod(add_mod(image[i*3+2],bg_delta,range),br_delta,range);
						rcache_L = range + image[i*3+1] - rg_delta;
						bcache_L = 2*range + image[i*3+2] - bg_delta - br_delta;
					}
					else if(i < width){
						image[i*3 + 1] = (image[i*3 + 1] + rcache_L + rg_delta) % range;
						uint8_t br_delta = delta(br,image[i*3+1]);
						image[i*3 + 2] = (image[i*3 + 2] + bcache_L + bg_delta + br_delta) % range;
						rcache[i - 1] = rcache_L;
						bcache[i - 1] = bcache_L;
						rcache_L = image[i*3 + 1] + range - rg_delta;
						bcache_L = image[i*3 + 2] + 2*range - bg_delta - br_delta;
					}
					else if(i % width == 0){
						image[i*3 + 1] = (image[i*3 + 1] + rcache[0] + rg_delta) % range;
						uint8_t br_delta = delta(br,image[i*3+1]);
						image[i*3 + 2] = (image[i*3 + 2] + bcache[0] + bg_delta + br_delta) % range;
						rcache[width - 1] = rcache_L;
						bcache[width - 1] = bcache_L;
						rcache_L = image[i*3 + 1] + range - rg_delta;
						bcache_L = image[i*3 + 2] + 2*range - bg_delta - br_delta;
					}
					else{
						uint16_t predictor = predictorImage[predictorIndex*3+1];
						int a = (predictor & 0b1111000000000000) >> 12;
						int b = (predictor & 0b0000111100000000) >> 8;
						int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
						int d = (predictor & 0b0000000000001111);

						uint8_t sum = a + b + c + d;
						uint8_t halfsum = sum >> 1;

						uint16_t L = rcache_L;
						uint16_t T = rcache[i % width];
						uint16_t TL = rcache[(i-1) % width];
						uint16_t TR = rcache[(i+1) % width];
						if(predictor == 0){
							image[i*3 + 1] = (
								image[i*3 + 1]
								+ ffv1_16(L,T,TL)
								+ rg_delta
							) % range;
						}
						else{
							image[i*3 + 1] = (
								image[i*3 + 1]
								+ i_clamp(
									(
										a*L + b*T + c*TL + d*TR + halfsum
									)/sum,
									0,
									2*range
								)
								+ rg_delta
							) % range;
						}
						rcache[(i - 1) % width] = rcache_L;
						rcache_L = image[i*3 + 1] + range - rg_delta;

						uint8_t br_delta = delta(br,image[i*3+1]);

						predictor = predictorImage[predictorIndex*3 + 2];
						a = (predictor & 0b1111000000000000) >> 12;
						b = (predictor & 0b0000111100000000) >> 8;
						c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
						d = (predictor & 0b0000000000001111);
						sum = a + b + c + d;
						halfsum = sum >> 1;
						L = bcache_L;
						T = bcache[i % width];
						TL = bcache[(i-1) % width];
						TR = bcache[(i+1) % width];
						if(predictor == 0){
							image[i*3 + 2] = (
								image[i*3 + 2]
								+ ffv1_16(L,T,TL)
								+ bg_delta
								+ br_delta
							) % range;
						}
						else{
							image[i*3 + 2] = (
								image[i*3 + 2]
								+ i_clamp(
									(
										a*L + b*T + c*TL + d*TR + halfsum
									)/sum,
									0,
									3*range
								)
								+ bg_delta
								+ br_delta
							) % range;
						}
						bcache[(i - 1) % width] = bcache_L;
						bcache_L = image[i*3 + 2] + 2*range - bg_delta - br_delta;
					}
				}
				else{
					image[i*3+1] = add_mod(image[i*3+1],rg_delta,range);
					image[i*3+2] = add_mod(add_mod(image[i*3+2],bg_delta,range),delta(br,image[i*3+1]),range);
				}
			}
		}
	}
	if(INDEX_TRANSFORM){
		for(size_t i=0;i<width*height;i++){
			size_t indexIndexIndex = tileIndexFromPixel(
				i,
				width,
				indexIndexWidth,
				indexIndexWidth_block,
				indexIndexHeight_block
			);
			uint8_t index_to_use = indexIndexImage[indexIndexIndex*3];
			uint8_t location = image[i*3];
			image[i*3]   = indexImage[(indexWidth*index_to_use + location)*3];
			image[i*3+1] = indexImage[(indexWidth*index_to_use + location)*3+1];
			image[i*3+2] = indexImage[(indexWidth*index_to_use + location)*3+2];
		}
	}
	printf("pixels decoded\n");

	if(INDEX_TRANSFORM){
		delete[] indexImage;
		delete[] indexIndexImage;
		delete[] indexLengths;
	}
	if(COLOUR_TRANSFORM){
		delete[] colourImage;
	}
	if(PREDICTION_MAP){
		delete[] predictorImage;
	}
	if(ENTROPY_MAP){
		delete[] entropyImage;
	}
	return image;
}

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}
	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	printf("read %d bytes\n",(int)in_size);

	uint8_t* fileIndex = in_bytes;
	uint32_t width =  readVarint(fileIndex) + 1;
	uint32_t height = readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* normal = readImage(fileIndex, 256, width, height);
	delete[] in_bytes;

	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height;i++){
		image[i*4]   = normal[i*3+1];
		image[i*4+1] = normal[i*3];
		image[i*4+2] = normal[i*3+2];
		image[i*4+3] = 255;
	}
	delete[] normal;


	encodeOneStep(argv[2], image, width, height);

	return 0;
}
