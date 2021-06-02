#ifndef ENTROPY_OPTIMISER_HEADER
#define ENTROPY_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "entropy_estimation.hpp"

/*
	try shuffling blocks around
	returns the new number of contexts
	modifies the entropy image and the stats
*/
uint8_t entropy_redistribution_pass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* entropy_image,
	uint8_t contexts,
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
		for(size_t pred=0;pred<contextNumber;pred++){
			regions[pred] = regionalEntropy(
				in_bytes,
				costTables[pred],
				i,
				width,
				height,
				entropy_width_block,
				entropy_height_block
			);
		}
		double best = regions[0];
		entropy_image[i] = 0;
		for(size_t pred=1;pred<contextNumber;pred++){
			if(regions[pred] < best){
				best = regions[pred];
				entropyImage[i] = pred;
			}
		}
		contextsUsed[entropyImage[i]]++;
	}
//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}
//shrink symbol stats available
	uint8_t index = 0;
	for(size_t i=0;i<contexts;i++){
		if(contextsUsed[i]){
			SymbolStats[index++] = SymbolStats[i];
		}
	}
	return index;//new number of contexts
}

#endif //ENTROPY_OPTIMISER
