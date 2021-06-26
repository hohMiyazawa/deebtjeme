#ifndef COLOUR_ENTROPY_OPTIMISER_HEADER
#define COLOUR_ENTROPY_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "entropy_estimation.hpp"
#include "table_encode.hpp"
#include "numerics.hpp"
#include "fake_encoders.hpp"

/*
	heuristic for entropy mapping
*/
uint8_t colour_entropy_map_initial(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& entropy_image,
	uint32_t& entropyWidth,
	uint32_t& entropyHeight,
	uint32_t entropyWidth_block,
	uint32_t entropyHeight_block,
	uint8_t contextNumber
){
	if(entropyWidth_block == 0){
		entropyWidth_block  = 8;
	}
	if(entropyHeight_block == 0){
		entropyHeight_block = 8;
	}

	entropyWidth  = (width + entropyWidth_block - 1)/entropyWidth_block;
	entropyHeight = (height + entropyWidth_block - 1)/entropyWidth_block;

	if(contextNumber == 0){
		if(((width + height) + 255)/256 > 255){
			contextNumber = 255;
		}
		else{
			contextNumber = ((width + height) + 255)/256;
		}
	}

	SymbolStats defaultFreqs;
	defaultFreqs.count_freqs(in_bytes, width*height*3);

	double* costTable = entropyLookup(defaultFreqs,width*height*3);

	double entropyMap[entropyWidth*entropyHeight*3];
	double sortedEntropies[entropyWidth*entropyHeight*3];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		double region0 = regionalEntropy(
			in_bytes,
			costTable,
			i,
			width,
			height,
			entropyWidth_block,
			entropyHeight_block,
			0//
		);
		double region1 = regionalEntropy(
			in_bytes,
			costTable,
			i,
			width,
			height,
			entropyWidth_block,
			entropyHeight_block,
			1//
		);
		double region2 = regionalEntropy(
			in_bytes,
			costTable,
			i,
			width,
			height,
			entropyWidth_block,
			entropyHeight_block,
			2//
		);
		entropyMap[i*3] = region0;
		entropyMap[i*3 + 1] = region1;
		entropyMap[i*3 + 2] = region2;
		sortedEntropies[i*3] = region0;
		sortedEntropies[i*3 + 1] = region1;
		sortedEntropies[i*3 + 2] = region2;
	}
	delete[] costTable;

	qsort(sortedEntropies, entropyWidth*entropyHeight*3, sizeof(double), compare);

	double pivots[contextNumber];
	for(size_t i=0;i<contextNumber;i++){
		pivots[i] = sortedEntropies[(entropyWidth*entropyHeight*3 * (i+1))/contextNumber - 1];
	}

	entropy_image = new uint8_t[entropyWidth*entropyHeight*3];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i*3] <= pivots[j]){
				entropy_image[i*3] = j;
				break;
			}
		}
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i*3+1] <= pivots[j]){
				entropy_image[i*3+1] = j;
				break;
			}
		}
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i*3+2] <= pivots[j]){
				entropy_image[i*3+2] = j;
				break;
			}
		}
	}
	return contextNumber;
}

/*
	try shuffling blocks around
	returns the new number of contexts
	modifies the entropy image and the stats
*/
uint32_t colour_entropy_redistribution_pass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& entropy_image,
	uint32_t contexts,
	uint32_t entropy_width,
	uint32_t entropy_height,
	SymbolStats* entropy_stats
){
	uint32_t entropy_width_block  = (width + entropy_width - 1)/entropy_width;
	uint32_t entropy_height_block = (height + entropy_height - 1)/entropy_height;

	double* costTables[contexts];

	for(size_t i=0;i<contexts;i++){
		costTables[i] = entropyLookup(entropy_stats[i]);
	}


	size_t contextsUsed[contexts];
	for(size_t i=0;i<contexts;i++){
		contextsUsed[i] = 0;
	}
	for(size_t i=0;i<entropy_width*entropy_height;i++){
		double regions[contexts];
		for(size_t pred=0;pred<contexts;pred++){
			regions[pred] = regionalEntropy(
				in_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropy_width_block,
				entropy_height_block,
				0
			);
		}
		double best = regions[0];
		entropy_image[i*3] = 0;
		for(size_t pred=1;pred<contexts;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropy_image[i*3] = pred;
			}
		}
		contextsUsed[entropy_image[i*3]]++;


		for(size_t pred=0;pred<contexts;pred++){
			regions[pred] = regionalEntropy(
				in_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropy_width_block,
				entropy_height_block,
				1
			);
		}
		best = regions[0];
		entropy_image[i*3 + 1] = 0;
		for(size_t pred=1;pred<contexts;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropy_image[i*3 + 1] = pred;
			}
		}
		contextsUsed[entropy_image[i*3 + 1]]++;
		for(size_t pred=0;pred<contexts;pred++){
			regions[pred] = regionalEntropy(
				in_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropy_width_block,
				entropy_height_block,
				2
			);
		}
		best = regions[0];
		entropy_image[i*3 + 2] = 0;
		for(size_t pred=1;pred<contexts;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropy_image[i*3 + 2] = pred;
			}
		}
		contextsUsed[entropy_image[i*3 + 2]]++;
	}
//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}
//update stats
	for(size_t context = 0;context < contexts;context++){
		for(size_t i=0;i<256;i++){
			entropy_stats[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropy_width,
			entropy_width_block,
			entropy_height_block
		);
		entropy_stats[entropy_image[tileIndex*3 + 0]].freqs[in_bytes[i*3 + 0]]++;
		entropy_stats[entropy_image[tileIndex*3 + 1]].freqs[in_bytes[i*3 + 1]]++;
		entropy_stats[entropy_image[tileIndex*3 + 2]].freqs[in_bytes[i*3 + 2]]++;
	}
//shrink symbol stats available
	uint32_t index = 0;
	uint8_t mapping[contexts];
	for(size_t i=0;i<contexts;i++){
		mapping[i] = index;
		if(contextsUsed[i]){
			entropy_stats[index++] = entropy_stats[i];
		}
	}
	for(size_t i=0;i<entropy_width*entropy_height;i++){
		entropy_image[i] = mapping[entropy_image[i]];
	}
	return index;//new number of contexts
}

size_t measure_entropy_efficiency(
	uint8_t* filtered_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* entropy_image,
	uint32_t contextNumber,
	uint32_t entropy_width,
	uint32_t entropy_height,
	uint32_t entropy_width_block,
	uint32_t entropy_height_block
){
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
			entropy_width,
			entropy_width_block,
			entropy_height_block
		);
		statistics[entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
		statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
		statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
	}

	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, range);
	}
	tableEncode.conclude();

	size_t image_size = 0;
	if(contextNumber > 1){
		image_size = colour_encode_combiner_dry(
			entropy_image,
			contextNumber,
			entropy_width,
			entropy_height
		);
	}

	double entropy_sum = 0;
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<range;i++){
			if(statistics[context].freqs[i]){
				entropy_sum += statistics[context].freqs[i] * (16 - std::log2((double)table[context].freqs[i]));
			}
		}
	}
	return (entropy_sum/8) + tableEncode.length + image_size;
}

uint32_t colour_contextSize_optimiser(
	uint8_t* filtered_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& entropy_image,
	uint32_t contexts,
	uint32_t& entropy_width,
	uint32_t& entropy_height,
	uint32_t& entropy_width_block,
	uint32_t& entropy_height_block,
	SymbolStats*& entropy_stats,
	size_t speed
){
	size_t default_size = measure_entropy_efficiency(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		contexts,
		entropy_width,
		entropy_height,
		entropy_width_block,
		entropy_height_block
	);
	size_t bestBlock = entropy_width_block;
	size_t bestBlockSize = 999999999;
	size_t bestBlockRange = 1;
	for(size_t block=4;block<=20;block++){
		uint8_t* b_entropy_image;

		uint32_t b_entropyWidth;
		uint32_t b_entropyHeight;

		uint8_t b_contextNumber = colour_entropy_map_initial(
			filtered_bytes,
			range,
			width,
			height,
			b_entropy_image,
			b_entropyWidth,
			b_entropyHeight,
			block,block,0
		);

		uint32_t b_entropyWidth_block  = (width + b_entropyWidth - 1)/b_entropyWidth;
		uint32_t b_entropyHeight_block  = (height + b_entropyHeight - 1)/b_entropyHeight;

		SymbolStats b_statistics[b_contextNumber];
		
		for(size_t context = 0;context < b_contextNumber;context++){
			for(size_t i=0;i<256;i++){
				b_statistics[context].freqs[i] = 0;
			}
		}
		for(size_t i=0;i<width*height;i++){
			size_t tile_index = tileIndexFromPixel(
				i,
				width,
				b_entropyWidth,
				b_entropyWidth_block,
				b_entropyHeight_block
			);
			b_statistics[b_entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
			b_statistics[b_entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
			b_statistics[b_entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
		}

		for(size_t i=0;i<speed;i++){
			b_contextNumber = colour_entropy_redistribution_pass(
				filtered_bytes,
				range,
				width,
				height,
				b_entropy_image,
				b_contextNumber,
				b_entropyWidth,
				b_entropyHeight,
				b_statistics
			);
		}

		size_t block_size = measure_entropy_efficiency(
			filtered_bytes,
			range,
			width,
			height,
			b_entropy_image,
			b_contextNumber,
			b_entropyWidth,
			b_entropyHeight,
			b_entropyWidth_block,
			b_entropyHeight_block
		);
		printf("%d block size: %d\n",(int)block,(int)block_size);
		if(block_size < bestBlockSize){
			bestBlockSize = block_size;
			bestBlock = block;
		}
	}
	bestBlockSize = 999999999;
	for(size_t blockRange = 1;blockRange < 20;blockRange++){
		uint8_t* b_entropy_image;

		uint32_t b_entropyWidth;
		uint32_t b_entropyHeight;

		uint8_t b_contextNumber = colour_entropy_map_initial(
			filtered_bytes,
			range,
			width,
			height,
			b_entropy_image,
			b_entropyWidth,
			b_entropyHeight,
			bestBlock,bestBlock,blockRange
		);

		uint32_t b_entropyWidth_block  = (width + b_entropyWidth - 1)/b_entropyWidth;
		uint32_t b_entropyHeight_block  = (height + b_entropyHeight - 1)/b_entropyHeight;

		SymbolStats b_statistics[b_contextNumber];
		
		for(size_t context = 0;context < b_contextNumber;context++){
			for(size_t i=0;i<256;i++){
				b_statistics[context].freqs[i] = 0;
			}
		}
		for(size_t i=0;i<width*height;i++){
			size_t tile_index = tileIndexFromPixel(
				i,
				width,
				b_entropyWidth,
				b_entropyWidth_block,
				b_entropyHeight_block
			);
			b_statistics[b_entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
			b_statistics[b_entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
			b_statistics[b_entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
		}

		for(size_t i=0;i<speed;i++){
			b_contextNumber = colour_entropy_redistribution_pass(
				filtered_bytes,
				range,
				width,
				height,
				b_entropy_image,
				b_contextNumber,
				b_entropyWidth,
				b_entropyHeight,
				b_statistics
			);
		}

		size_t block_size = measure_entropy_efficiency(
			filtered_bytes,
			range,
			width,
			height,
			b_entropy_image,
			b_contextNumber,
			b_entropyWidth,
			b_entropyHeight,
			b_entropyWidth_block,
			b_entropyHeight_block
		);
		printf("%d block@%d size: %d\n",(int)bestBlock,(int)blockRange,(int)block_size);
		if(block_size < bestBlockSize){
			bestBlockSize = block_size;
			bestBlockRange = blockRange;
		}
	}
	printf("found block %d@%d: %d\n",(int)bestBlock,(int)bestBlockRange,(int)bestBlockSize);
	printf("default block size: %d\n",(int)default_size);
	if(bestBlockSize < default_size){
		printf("replacing entropy density map\n");
		delete[] entropy_image;

		contexts = colour_entropy_map_initial(
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			entropy_width,
			entropy_height,
			bestBlock,bestBlock,bestBlockRange
		);

		entropy_width_block  = (width + entropy_width - 1)/entropy_width;
		entropy_height_block  = (height + entropy_height - 1)/entropy_height;

		SymbolStats* statistics = new SymbolStats[contexts];
		
		for(size_t context = 0;context < contexts;context++){
			for(size_t i=0;i<256;i++){
				statistics[context].freqs[i] = 0;
			}
		}
		for(size_t i=0;i<width*height;i++){
			size_t tile_index = tileIndexFromPixel(
				i,
				width,
				entropy_width,
				entropy_width_block,
				entropy_height_block
			);
			statistics[entropy_image[tile_index*3]].freqs[filtered_bytes[i*3]]++;
			statistics[entropy_image[tile_index*3 + 1]].freqs[filtered_bytes[i*3 + 1]]++;
			statistics[entropy_image[tile_index*3 + 2]].freqs[filtered_bytes[i*3 + 2]]++;
		}

		for(size_t i=0;i<speed + 5;i++){
			contexts = colour_entropy_redistribution_pass(
				filtered_bytes,
				range,
				width,
				height,
				entropy_image,
				contexts,
				entropy_width,
				entropy_height,
				statistics
			);
		}
		entropy_stats = statistics;
	}
	return contexts;
}

#endif //COLOUR_ENTROPY_OPTIMISER
