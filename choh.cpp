#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <cmath>
#include <math.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "filters.hpp"
#include "2dutils.hpp"
#include "lode_io.hpp"
#include "varint.hpp"
#include "laplace.hpp"
#include "numerics.hpp"
#include "bitwriter.hpp"
#include "table_encode.hpp"
#include "entropy_estimation.hpp"
#include "optimiser.hpp"
#include "entropy_coding.hpp"
#include "simple_encoders.hpp"
#include "colour_simple_encoders.hpp"
#include "colour_optimiser.hpp"
#include "research_optimiser.hpp"

void print_usage(){
	printf("./choh infile.png outfile.hoh speed\n\nspeed is a number from 0-8\nGreyscale only (the G component of a PNG file be used for RGB input)\n");
}

void encode_static_ffv1(uint8_t* in_bytes,size_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){


	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	SymbolStats table = laplace(8,range);

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

	*(--outPointer) = 0b00001000;//use table number 8
	*(--outPointer) = 0;
	*(--outPointer) = 0;
	*(--outPointer) = 0;
	*(--outPointer) = 0b00000100;
}

void encode_fewPass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t predictorCount = speed*2;
	if(predictorCount > 11){
		predictorCount = 11;
	}
	uint16_t predictorSelection[11] = {
		0,//ffv1
		0b0001000111010000,//avg L-T
		0b0001000111000000,//(1,1,-1,0)
		0b0011000111000001,//(3,1,-1,1)

		0b0001001111000000,//(1,3,-1,0)
		0b0011000011000010,//(3,0,-1,2)
		0b0011000011000001,//(3,0,-1,1)
		0b0010000111000000,//(2,1,-1,0)
		0b0001001011000000,//(1,2,-1,0)
		0b0001000011010000,//(1,0,0,0)
		0b0000000111010000//(0,1,0,0)
	};

	uint32_t predictorWidth_block = 8;
	uint32_t predictorHeight_block = 8;
	uint32_t predictorWidth = (width + predictorWidth_block - 1)/predictorWidth_block;
	uint32_t predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;

	uint8_t *filtered_bytes[predictorCount];

	for(size_t i=0;i<predictorCount;i++){
		filtered_bytes[i] = filter_all(in_bytes, range, width, height, predictorSelection[i]);
	}

	SymbolStats defaultFreqs;
	defaultFreqs.count_freqs(filtered_bytes[0], width*height);

	double* costTable = entropyLookup(defaultFreqs,width*height);

	uint8_t* predictorImage = new uint8_t[predictorWidth*predictorHeight];

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTable,
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	SymbolStats predictorsUsed;
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}

	delete[] costTable;

	printf("merging filtered bitplanes\n");
	for(size_t i=0;i<width*height;i++){
		filtered_bytes[0][i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}
	for(size_t i=1;i<predictorCount;i++){
		delete[] filtered_bytes[i];
	}

	printf("starting entropy mapping\n");

	uint32_t entropyWidth  = (width)/128;
	uint32_t entropyHeight = (height)/128;
	if(entropyWidth == 0){
		entropyWidth = 1;
	}
	if(entropyHeight == 0){
		entropyHeight = 1;
	}
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	SymbolStats stats[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		stats[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)].freqs[filtered_bytes[0][i]]++;
	}

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		entropyImage[i] = i;//all contexts are unique
	}

	BitWriter tableEncode;
	SymbolStats table[entropyWidth*entropyHeight];
	for(size_t context = 0;context < entropyWidth*entropyHeight;context++){
		table[context] = encode_freqTable(stats[context],tableEncode, range);
	}
	tableEncode.conclude();

	entropyCoding_map(
		filtered_bytes[0],
		width,
		height,
		table,
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] filtered_bytes[0];

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	encode_ranged_simple2(
		entropyImage,
		entropyWidth*entropyHeight,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropyImage;

	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);

	*(--outPointer) = entropyWidth*entropyHeight - 1;//number of contexts

	encode_ranged_simple2(
		predictorImage,
		predictorCount,
		predictorWidth,
		predictorHeight,
		outPointer
	);
	delete[] predictorImage;

	writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictorSelection[i] % 256;
		*(--outPointer) = predictorSelection[i] >> 8;
	}
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b00000110;//use prediction and entropy coding with map
}

void encode_optimiser2(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t predictorCount = speed*2;
	if(predictorCount > 13){
		predictorCount = 13;
	}
	uint16_t predictorSelection[13] = {
		0,//ffv1
		0b0001000111010000,//avg L-T
		0b0001000111000000,//(1,1,-1,0)
		0b0011000111000001,//(3,1,-1,1)
		0b0001001111000000,//(1,3,-1,0)
		0b0011000011000010,//(3,0,-1,2)
		0b0010000111000001,//(2,1,-1,1)
		0b0011001011000000,//(3,2,-1,0)
		0b0011000011000001,//(3,0,-1,1)
		0b0010000111000000,//(2,1,-1,0)
		0b0001001011000000,//(1,2,-1,0)
		0b0001000011010000,//(1,0,0,0)
		0b0000000111010000//(0,1,0,0)
	};

	uint32_t predictorWidth_block = 8;
	uint32_t predictorHeight_block = 8;
	uint32_t predictorWidth = (width + predictorWidth_block - 1)/predictorWidth_block;
	uint32_t predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;

	uint8_t *filtered_bytes[predictorCount];

	for(size_t i=0;i<predictorCount;i++){
		filtered_bytes[i] = filter_all(in_bytes, range, width, height, predictorSelection[i]);
	}

	SymbolStats defaultFreqs;
	defaultFreqs.count_freqs(filtered_bytes[0], width*height);

	double* costTable = entropyLookup(defaultFreqs,width*height);

	uint8_t* predictorImage = new uint8_t[predictorWidth*predictorHeight];

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTable,
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	SymbolStats predictorsUsed;
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}

	delete[] costTable;

	printf("merging filtered bitplanes\n");
	uint8_t* final_bytes = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		final_bytes[i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}

	printf("starting entropy mapping\n");

	uint32_t entropyWidth_block  = 8;
	uint32_t entropyHeight_block = 8;

	uint32_t entropyWidth  = (width + 7)/8;
	uint32_t entropyHeight = (height + 7)/8;

	uint8_t contextNumber = (width + height + 255)/256;

	printf("entropy map %d x %d\n",(int)entropyWidth,(int)entropyHeight);
	printf("block size %d x %d\n",(int)entropyWidth_block,(int)entropyHeight_block);

	defaultFreqs.count_freqs(final_bytes, width*height);

	costTable = entropyLookup(defaultFreqs,width*height);

	double entropyMap[entropyWidth*entropyHeight];
	double sortedEntropies[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		double region = regionalEntropy(
			final_bytes,
			costTable,
			i,
			width,
			height,
			entropyWidth_block,
			entropyHeight_block
		);
		entropyMap[i] = region;
		sortedEntropies[i] = region;
	}
	delete[] costTable;

	qsort(sortedEntropies, entropyWidth*entropyHeight, sizeof(double), compare);

	double pivots[contextNumber];
	for(size_t i=0;i<contextNumber;i++){
		pivots[i] = sortedEntropies[(entropyWidth*entropyHeight * (i+1))/contextNumber - 1];
		//printf("pivot: %f\n",pivots[i]);
	}

	uint8_t* entropyImage = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i] <= pivots[j]){
				entropyImage[i] = j;
				break;
			}
		}
	}

	SymbolStats contextsUsed;
	contextsUsed.count_freqs(entropyImage, entropyWidth*entropyHeight);

	/*for(size_t i=0;i<contextNumber;i++){
		printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
	}*/
	printf("---\n");

	SymbolStats stats[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}

	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}

	double* costTables[contextNumber];

	for(size_t i=0;i<contextNumber;i++){
		costTables[i] = entropyLookup(stats[i]);
	}

	for(size_t i=0;i<contextNumber;i++){
		contextsUsed.freqs[i] = 0;
	}
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		double regions[contextNumber];
		for(size_t pred=0;pred<contextNumber;pred++){
			regions[pred] = regionalEntropy(
				final_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropyWidth_block,
				entropyHeight_block
			);
		}
		double best = regions[0];
		entropyImage[i] = 0;
		for(size_t pred=1;pred<contextNumber;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropyImage[i] = pred;
			}
		}
		contextsUsed.freqs[entropyImage[i]]++;
	}
	for(size_t i=0;i<contextNumber;i++){
		printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
	}

	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}

	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}
	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
		costTables[i] = entropyLookup(stats[i]);
	}

	//predictor optimisation
	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		double regions[predictorCount];
		for(size_t pred=0;pred<predictorCount;pred++){
			regions[pred] = regionalEntropy(
				filtered_bytes[pred],
				costTables[entropyImage[i]],//assumes predictor block size is equal to entropy block size
				i,
				width,
				height,
				predictorWidth_block,
				predictorHeight_block
			);
		}
		double best = regions[0];
		predictorImage[i] = 0;
		for(size_t pred=1;pred<predictorCount;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				predictorImage[i] = pred;
			}
		}
	}
	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}
	predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

	for(size_t i=0;i<predictorCount;i++){
		printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
	}
	printf("merging filtered bitplanes again\n");
	for(size_t i=0;i<width*height;i++){
		final_bytes[i] = filtered_bytes[predictorImage[tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		)]][i];
	}
	//possible loop start?
	for(size_t loop=0;loop < (int)speed - 3;loop++){
		for(size_t context = 0;context < contextNumber;context++){
			for(size_t i=0;i<256;i++){
				stats[context].freqs[i] = 0;
			}
		}

		for(size_t i=0;i<width*height;i++){
			uint8_t cntr = entropyImage[tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			)];
			stats[cntr].freqs[final_bytes[i]]++;
		}

		for(size_t i=0;i<contextNumber;i++){
			costTables[i] = entropyLookup(stats[i]);
		}

		for(size_t i=0;i<contextNumber;i++){
			contextsUsed.freqs[i] = 0;
		}
		for(size_t i=0;i<entropyWidth*entropyHeight;i++){
			double regions[contextNumber];
			for(size_t pred=0;pred<contextNumber;pred++){
				regions[pred] = regionalEntropy(
					final_bytes,
					costTables[pred],
					i,
					width,
					height,
					entropyWidth_block,
					entropyHeight_block
				);
			}
			double best = regions[0];
			entropyImage[i] = 0;
			for(size_t pred=1;pred<contextNumber;pred++){
				if(regions[pred] < best){
					best = regions[pred];
					entropyImage[i] = pred;
				}
			}
			contextsUsed.freqs[entropyImage[i]]++;
		}
		for(size_t i=0;i<contextNumber;i++){
			printf("entr %d: %d\n",(int)i,(int)contextsUsed.freqs[i]);
		}

		for(size_t context = 0;context < contextNumber;context++){
			for(size_t i=0;i<256;i++){
				stats[context].freqs[i] = 0;
			}
		}

		for(size_t i=0;i<width*height;i++){
			uint8_t cntr = entropyImage[tileIndexFromPixel(
				i,
				width,
				entropyWidth,
				entropyWidth_block,
				entropyHeight_block
			)];
			stats[cntr].freqs[final_bytes[i]]++;
		}
		for(size_t i=0;i<contextNumber;i++){
			delete[] costTables[i];
			costTables[i] = entropyLookup(stats[i]);
		}

		//predictor optimisation
		for(size_t i=0;i<predictorWidth*predictorHeight;i++){
			double regions[predictorCount];
			for(size_t pred=0;pred<predictorCount;pred++){
				regions[pred] = regionalEntropy(
					filtered_bytes[pred],
					costTables[entropyImage[i]],//assumes predictor block size is equal to entropy block size
					i,
					width,
					height,
					predictorWidth_block,
					predictorHeight_block
				);
			}
			double best = regions[0];
			predictorImage[i] = 0;
			for(size_t pred=1;pred<predictorCount;pred++){
				if(regions[pred] < best){
					best = regions[pred];
					predictorImage[i] = pred;
				}
			}
		}
		for(size_t i=0;i<contextNumber;i++){
			delete[] costTables[i];
		}
		predictorsUsed.count_freqs(predictorImage, predictorWidth*predictorHeight);

		for(size_t i=0;i<predictorCount;i++){
			printf("pred %d: %d\n",(int)i,(int)predictorsUsed.freqs[i]);
		}
		printf("merging filtered bitplanes again\n");
		for(size_t i=0;i<width*height;i++){
			final_bytes[i] = filtered_bytes[predictorImage[tileIndexFromPixel(
				i,
				width,
				predictorWidth,
				predictorWidth_block,
				predictorHeight_block
			)]][i];
		}
	}
	//possible loop end?
	for(size_t i=0;i<predictorCount;i++){
		delete[] filtered_bytes[i];
	}

	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			stats[context].freqs[i] = 0;
		}
	}
/* progressive research
	for(size_t i = 1;i<width*height;i++){
		size_t y = i / width;
		size_t x = i % width;
		if(y % 128 == 0){
			final_bytes[i] = sub_mod(in_bytes[i],in_bytes[i - 1],range);
		}
		else if(x % 128 == 0){
			final_bytes[i] = sub_mod(in_bytes[i],in_bytes[i - width],range);
		}
	}
*/
/*
	for(size_t i = 2;i<width*height;i++){
		size_t y = i / width;
		size_t x = i % width;
		if(y % 32 == 0){
			final_bytes[i] = sub_mod(in_bytes[i],(4*(int)in_bytes[i - 1] - (int)in_bytes[i - 2])/3,range);
		}
		else if(x % 32 == 0){
			final_bytes[i] = sub_mod(in_bytes[i],in_bytes[i - width],range);
		}
	}
*/
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropyImage[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		stats[cntr].freqs[final_bytes[i]]++;
	}
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(stats[context],tableEncode, range);
	}
	tableEncode.conclude();

	entropyCoding_map(
		final_bytes,
		width,
		height,
		table,
		entropyImage,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] final_bytes;

	uint8_t* trailing = outPointer;;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	encode_ranged_simple2(
		entropyImage,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	delete[] entropyImage;
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	trailing = outPointer;
	encode_ranged_simple2(
		predictorImage,
		predictorCount,
		predictorWidth,
		predictorHeight,
		outPointer
	);
	writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
	delete[] predictorImage;
	printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictorSelection[i] % 256;
		*(--outPointer) = predictorSelection[i] >> 8;
	}
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b00000110;//use prediction and entropy coding with map
}

int main(int argc, char *argv[]){
	if(argc < 4){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	size_t speed = atoi(argv[3]);

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);
	bool greyscale = greyscale_test(decoded,width,height);
	if(greyscale){
		printf("converting to greyscale\n");
		uint8_t* grey = new uint8_t[width*height];
		for(size_t i=0;i<width*height;i++){
			grey[i] = decoded[i*4 + 1];
		}
		delete[] decoded;

		printf("creating buffer\n");

		size_t max_elements = width*height + 4096;
		uint8_t* out_buf = new uint8_t[max_elements];
		uint8_t* out_end = out_buf + max_elements;
		uint8_t* outPointer = out_end;

		if(speed == 0){
			encode_static_ffv1(grey, 256,width,height,outPointer);
		}
		else if(speed == 1){
			encode_ffv1(grey, 256,width,height,outPointer);
		}
		else if(speed < 3){
			encode_fewPass(grey, 256,width,height,outPointer, speed);
		}
		else if(speed < 5){
			encode_optimiser2(grey, 256,width,height,outPointer, speed);
		}
		else if(speed == 5){
			optimiser_speed(grey, 256,width,height,outPointer, 2);
		}
		else if(speed == 6){
			optimiser_speed(grey, 256,width,height,outPointer, 10);
		}
		else if(speed == 7){
			optimiser_speed2(grey, 256,width,height,outPointer, 15);
		}
		else if(speed == 8){
			optimiser_speed2(grey, 256,width,height,outPointer, 20);
		}
		else{
			optimiser(grey, 256,width,height,outPointer, speed);
		}
		delete[] grey;

		printf("writing header\n");
		writeVarint_reverse((uint32_t)(height - 1),outPointer);
		writeVarint_reverse((uint32_t)(width - 1), outPointer);

		
		printf("file size %d\n",(int)(out_end - outPointer));


		write_file(argv[2],outPointer,out_end - outPointer);
		delete[] out_buf;
	}
	else{
		uint8_t* alpha_stripped = new uint8_t[width*height*3];
		for(size_t i=0;i<width*height;i++){
			alpha_stripped[i*3 + 0] = decoded[i*4 + 1];
			alpha_stripped[i*3 + 1] = decoded[i*4 + 0];
			alpha_stripped[i*3 + 2] = decoded[i*4 + 2];
		}
		delete[] decoded;

		printf("creating buffer\n");

		size_t max_elements = (width*height + 4096)*3;
		uint8_t* out_buf = new uint8_t[max_elements];
		uint8_t* out_end = out_buf + max_elements;
		uint8_t* outPointer = out_end;

		if(speed == 0){
			colour_encode_entropy_channel(alpha_stripped, 256,width,height,outPointer);
		}
		else if(speed == 1){
			colour_encode_ffv1(alpha_stripped, 256,width,height,outPointer);
		}
		else if(speed == 2){
			colour_encode_ffv1_4x4(alpha_stripped, 256,width,height,outPointer);
		}
		else if(speed == 3){
			//colour_optimiser_entropyOnly(alpha_stripped, 256,width,height,outPointer, 1);
			colour_optimiser_take0a(alpha_stripped, 256,width,height,outPointer, 1);
		}
		else if(speed == 4){
			//colour_optimiser_entropyOnly(alpha_stripped, 256,width,height,outPointer, 5);
			colour_optimiser_take0(alpha_stripped, 256,width,height,outPointer, 1);
		}
		else if(speed == 5){
			colour_optimiser_take1(alpha_stripped, 256,width,height,outPointer, 5);
		}
		else if(speed == 6){
			//colour_optimiser_take3(alpha_stripped, 256,width,height,outPointer, 5);
			colour_optimiser_take3_lz(alpha_stripped, 256,width,height,outPointer, 5);
		}
		else if(speed == 7){
			//colour_optimiser_take4(alpha_stripped, 256,width,height,outPointer, 6);
			colour_optimiser_take4_lz(alpha_stripped, 256,width,height,outPointer, 6);
		}
		else if(speed == 8){
			colour_optimiser_take5_lz(alpha_stripped, 256,width,height,outPointer, 7);
		}
		else if(speed == 69){
			research_colour_writeEntImage(alpha_stripped, 256,width,height,outPointer, 7);
		}
		else{
			colour_optimiser_take6_lz(alpha_stripped, 256,width,height,outPointer, speed);
		}
		delete[] alpha_stripped;

		printf("writing header\n");
		writeVarint_reverse((uint32_t)(height - 1),outPointer);
		writeVarint_reverse((uint32_t)(width - 1), outPointer);

		
		printf("file size %d\n",(int)(out_end - outPointer));


		write_file(argv[2],outPointer,out_end - outPointer);
		delete[] out_buf;
	}

	return 0;
}
