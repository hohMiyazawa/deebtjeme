#ifndef COLOUR_ENTROPY_OPTIMISER_HEADER
#define COLOUR_ENTROPY_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "entropy_estimation.hpp"
#include "table_encode.hpp"
#include "numerics.hpp"

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
	uint32_t& entropyHeight
){
	uint32_t entropyWidth_block  = 8;
	uint32_t entropyHeight_block = 8;

	entropyWidth  = (width + entropyWidth_block - 1)/entropyWidth_block;
	entropyHeight = (height + entropyWidth_block - 1)/entropyWidth_block;

	uint8_t contextNumber;
	if(((width + height) + 255)/256 > 255){
		contextNumber = 255;
	}
	else{
		contextNumber = ((width + height) + 255)/256;
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

#endif //COLOUR_ENTROPY_OPTIMISER
