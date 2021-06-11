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

	entropyWidth  = (width + 7)/8;
	entropyHeight = (height + 7)/8;

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

#endif //COLOUR_ENTROPY_OPTIMISER
