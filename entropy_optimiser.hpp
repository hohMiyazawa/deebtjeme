#ifndef ENTROPY_OPTIMISER_HEADER
#define ENTROPY_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "entropy_estimation.hpp"
#include "table_encode.hpp"

/*
	heuristic for entropy mapping
*/
uint8_t entropy_map_initial(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* entropy_image
){
	uint32_t entropyWidth_block  = 8;
	uint32_t entropyHeight_block = 8;

	uint32_t entropyWidth  = (width + 7)/8;
	uint32_t entropyHeight = (height + 7)/8;

	uint8_t contextNumber;
	if((width + height + 255)/256 > 255){
		contextNumber = 255;
	}
	else{
		contextNumber = (width + height + 255)/256;
	}

	SymbolStats defaultFreqs;
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
	}

	entropy_image = new uint8_t[entropyWidth*entropyHeight];
	for(size_t i=0;i<entropyWidth*entropyHeight;i++){
		for(size_t j=0;j<contextNumber;j++){
			if(entropyMap[i] <= pivots[j]){
				entropyImage[i] = j;
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
uint32_t entropy_redistribution_pass(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* entropy_image,
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
	uint32_t index = 0;
	for(size_t i=0;i<contexts;i++){
		if(contextsUsed[i]){
			SymbolStats[index++] = SymbolStats[i];
		}
	}
	return index;//new number of contexts
}

/* check impact of merging two contexts. negative means they will benefit from merging */
double context_merging(
	SymbolStats stats1,
	SymbolStats stats2,
	uint32_t range
){
	double initial = estimateEntropy_overhead(stats1, range) + estimateEntropy_overhead(stats2, range);
	SymbolStats merged;
	for(size_t i=0;i<256;i++){
		merged.freqs[i] = stats1.freqs[i] + stats2.freqs[i];
	}
	return estimateEntropy_overhead(merged, range) - initial;
}

#endif //ENTROPY_OPTIMISER
