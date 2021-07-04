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
	bool PROGRESSIVE,
	bool HAS_COLOUR,
	bool SUBTRACT_GREEN,
	bool INDEX_TRANSFORM,
	bool COLOUR_TRANSFORM,
	bool PREDICTION_MAP,
	bool ENTROPY_MAP,
	bool LZ
){
	if(PROGRESSIVE){
		panic("PROGRESSIVE bit set!\n");
	}
	if(!HAS_COLOUR && SUBTRACT_GREEN){
		panic("Can not subtract green for greyscale!\n");
	}
	if(!HAS_COLOUR && COLOUR_TRANSFORM){
		panic("Can not use colour transform for greyscale!\n");
	}
	if(SUBTRACT_GREEN && COLOUR_TRANSFORM){
		panic("Can not use colour transform and subtract green at the same time!\n");
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

	bool PROGRESSIVE      = (compressionMode & 0b10000000) >> 7;
	bool HAS_COLOUR       = (compressionMode & 0b01000000) >> 6;
	bool SUBTRACT_GREEN   = (compressionMode & 0b00100000) >> 5;
	bool INDEX_TRANSFORM  = (compressionMode & 0b00010000) >> 4;
	bool COLOUR_TRANSFORM = (compressionMode & 0b00001000) >> 3;
	bool PREDICTION_MAP   = (compressionMode & 0b00000100) >> 2;
	bool ENTROPY_MAP      = (compressionMode & 0b00000010) >> 1;
	bool LZ               = (compressionMode & 0b00000001) >> 0;

	sanity_check(
		PROGRESSIVE,
		HAS_COLOUR,
		SUBTRACT_GREEN,
		INDEX_TRANSFORM,
		COLOUR_TRANSFORM,
		PREDICTION_MAP,
		ENTROPY_MAP,
		LZ
	);
	uint8_t* indexImage;
	uint32_t indexWidth;
	uint32_t indexHeight;
	if(INDEX_TRANSFORM){
		uint8_t* trailing = fileIndex;
		indexWidth = *(fileIndex++) + 1;
		indexImage = readImage(fileIndex, 256, indexWidth, 1);
		if(indexWidth == 1){
			uint8_t* image = new uint8_t[width*height*3];
			for(size_t i=0;i<width*height;i++){
				image[i*3] = indexImage[0];
				image[i*3+1] = indexImage[1];
				image[i*3+2] = indexImage[2];
			}
			return image;
		}
		//printf("index %d,%d,%d %d,%d,%d\n",(int)indexImage[0],(int)indexImage[1],(int)indexImage[2],(int)indexImage[3],(int)indexImage[4],(int)indexImage[5]);
		range = indexWidth;
		printf("---palette size: %d bytes\n",(int)(fileIndex - trailing));
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
		printf("---start colour transform image\n");
		printf("  colour transform image %d x %d\n",(int)colourWidth,(int)colourHeight);
		colourImage = readImage(fileIndex, 256,colourWidth,colourHeight);
		printf("---colour transform image size: %d bytes\n",(int)(fileIndex - trailing));
	}
	else if(SUBTRACT_GREEN){
		colourWidth = 1;
		colourHeight = 1;
		colourWidth_block  = width;
		colourHeight_block = height;
		colourImage = new uint8_t[3];
		colourImage[0] = 255;
		colourImage[1] = 255;
		colourImage[2] = 0;
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
			printf("---start predictor image\n");
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
		printf("---start entropy image\n");
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

	RansDecSymbol dsyms[entropyContexts][256];

	SymbolStats backref_table;
	SymbolStats matchlen_table;
	SymbolStats futureref_table;
	RansDecSymbol dba[256];
	RansDecSymbol dml[256];
	RansDecSymbol dfr[256];

	uint8_t max_ba_table_prefix = 200;
	uint8_t max_ml_table_prefix = 40;
	uint8_t max_fr_table_prefix = 40;

	if(LZ){
		BitReader reader(&fileIndex);
		backref_table   = decode_freqTable(reader, max_ba_table_prefix);
		matchlen_table  = decode_freqTable(reader, max_ml_table_prefix);
		futureref_table = decode_freqTable(reader, max_fr_table_prefix);
	}
	RansState rans;
	RansDecSymbol decode_binary_zero;
	RansDecSymbol decode_binary_one;
	RansDecSymbolInit(&decode_binary_zero, 0, (1 << 15));
	RansDecSymbolInit(&decode_binary_one, (1 << 15), (1 << 15));
	if(ENTROPY_MAP){
		RansDecInit(&rans, &fileIndex);
		for(size_t cont = 0;cont < entropyContexts;cont++){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
			}
		}
		if(LZ){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dba[i], backref_table.cum_freqs[i], backref_table.freqs[i]);
				RansDecSymbolInit(&dml[i], matchlen_table.cum_freqs[i], matchlen_table.freqs[i]);
				RansDecSymbolInit(&dfr[i], futureref_table.cum_freqs[i], futureref_table.freqs[i]);
			}
			uint8_t lz_next_prefix = read_prefixcode(rans, dfr, futureref_table, fileIndex);
			printf("\n+++\n\n");
			printf("lz next initial prefix %d\n",(int)lz_next_prefix);
			lz_next = prefix_to_val(lz_next_prefix, rans, fileIndex, decode_binary_zero, decode_binary_one);
		}
	}

	uint16_t rcache[width];
	uint16_t bcache[width];
	uint16_t rcache_L = 0;
	uint16_t bcache_L = 0;

	uint8_t* image = new uint8_t[width*height*3];
	printf("decoding pixels\n");
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			//printf("index %d\n",(int)i);

			size_t backref;
			uint8_t backref_prefix = read_prefixcode(rans, dba, backref_table, fileIndex);
			if(backref_prefix < 120){
				backref = lut_y[backref_prefix]*width + lut_x[backref_prefix];
			}
			else if(backref_prefix < 140){
				size_t vertical = prefix_to_val(backref_prefix - 120, rans, fileIndex, decode_binary_zero, decode_binary_one);
				uint8_t xoffset = read32(rans, fileIndex, decode_binary_zero, decode_binary_one);
				backref = (vertical + 1) * width - xoffset + 16;
				//printf("xoff: %d %d %d\n",(int)backref,(int)xoffset,(int)(vertical + 1));
			}
			else if(backref_prefix < 160){
				size_t vertical = prefix_to_val(backref_prefix - 140, rans, fileIndex, decode_binary_zero, decode_binary_one);
				backref = (vertical + 1) * width;
			}
			else{
				backref = prefix_to_val(backref_prefix - 160, rans, fileIndex, decode_binary_zero, decode_binary_one) + 1;
			}

			uint8_t matchlen_prefix = read_prefixcode(rans, dml, matchlen_table, fileIndex);
			size_t matchlen;
			matchlen = prefix_to_val(matchlen_prefix, rans, fileIndex, decode_binary_zero, decode_binary_one);
			//printf("what matchlen %d(%d)\n",(int)matchlen_prefix,(int)matchlen);
			matchlen += 1;
			uint8_t lz_next_prefix = read_prefixcode(rans, dfr, futureref_table, fileIndex);
			lz_next = prefix_to_val(lz_next_prefix, rans, fileIndex, decode_binary_zero, decode_binary_one);

			//printf("what next %d\n",(int)lz_next);
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
				if((COLOUR_TRANSFORM || SUBTRACT_GREEN) && PREDICTION_MAP){
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
			//printf("lz triple\n");
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

		if(INDEX_TRANSFORM == 1){
		}
		else if(HAS_COLOUR == 0){
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
			if(COLOUR_TRANSFORM == 0 && SUBTRACT_GREEN == 0){
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
			uint8_t location = image[i*3];
			image[i*3]   = indexImage[location*3];
			image[i*3+1] = indexImage[location*3+1];
			image[i*3+2] = indexImage[location*3+2];
		}
	}
	printf("pixels decoded\n");

	if(INDEX_TRANSFORM){
		delete[] indexImage;
	}
	if(COLOUR_TRANSFORM || SUBTRACT_GREEN){
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
