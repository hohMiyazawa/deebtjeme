#ifndef ENTROPY_ESTIMATION_HEADER
#define ENTROPY_ESTIMATION_HEADER

#include "symbolstats.hpp"
#include "table_encode.hpp"

double estimateEntropy(uint8_t* in_bytes, size_t size){
	uint8_t frequencies[size];
	for(size_t i=0;i<256;i++){
		frequencies[i] = 0;
	}
	for(size_t i=0;i<size;i++){
		frequencies[in_bytes[i]]++;
	}
	double sum = 0;
	for(size_t i=0;i<256;i++){
		if(frequencies[i]){
			sum += -std::log2((double)frequencies[i]/(double)size) * (double)frequencies[i];
		}
	}
	return sum;
}

double estimateEntropy_freq(SymbolStats frequencies, size_t size){
	double sum = 0;
	for(size_t i=0;i<256;i++){
		if(frequencies.freqs[i]){
			sum += -std::log2((double)frequencies.freqs[i]/(double)size) * (double)frequencies.freqs[i];
		}
	}
	return sum;
}

double* entropyLookup(SymbolStats stats,size_t total){
	double* table = new double[256];
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i] == 0){
			table[i] = -std::log2(1/(double)total);
		}
		else{
			table[i] = -std::log2(stats.freqs[i]/(double)total);
		}
	}
	return table;
}

double estimateEntropy_overhead(SymbolStats stats1, uint32_t range){
	size_t overhead1;
	SymbolStats codedTable1 = encode_freqTable_dry(stats1, overhead1, range);
	double* cost1 = entropyLookup(codedTable1,1 << 16);
	double collected = overhead1;
	for(size_t i=0;i<range;i++){
		if(stats1.freqs[i]){
			collected += stats1.freqs[i] * cost1[i];
		}
	}
	delete[] cost1;
	return collected;
}

double* entropyLookup(SymbolStats stats){
	double* table = new double[256];
	size_t total = 0;
	for(size_t i=0;i<256;i++){
		total += stats.freqs[i];
	}
	if(total == 0){
		for(size_t i=0;i<256;i++){
			table[i] = 0;;
		}
		return table;
	}
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i] == 0){
			table[i] = -std::log2(1/(double)total);
		}
		else{
			table[i] = -std::log2(stats.freqs[i]/(double)total);
		}
	}
	return table;
}

double regionalEntropy(
	uint8_t* in_bytes,
	double* entropyTable,
	size_t tileIndex,
	uint32_t width,
	uint32_t height,
	uint32_t b_width_block,
	uint32_t b_height_block
){
	uint32_t b_width = (width + b_width_block - 1)/b_width_block;
	double sum = 0;
	for(size_t y=0;y<b_height_block;y++){
		uint32_t y_pos = (tileIndex / b_width)*b_height_block + y;
		if(y >= height){
			continue;
		}
		for(size_t x=0;x<b_width_block;x++){
			uint32_t x_pos = (tileIndex % b_width)*b_width_block + x;
			if(x >= width){
				continue;
			}
			sum += entropyTable[in_bytes[y_pos * width + x_pos]];
		}
	}
	return sum;
}

double regionalEntropy(
	uint8_t* in_bytes,
	double* entropyTable,
	size_t tileIndex,
	uint32_t width,
	uint32_t height,
	uint32_t b_width_block,
	uint32_t b_height_block,
	uint8_t offset
){
	uint32_t b_width = (width + b_width_block - 1)/b_width_block;
	double sum = 0;
	for(size_t y=0;y<b_height_block;y++){
		uint32_t y_pos = (tileIndex / b_width)*b_height_block + y;
		if(y >= height){
			continue;
		}
		for(size_t x=0;x<b_width_block;x++){
			uint32_t x_pos = (tileIndex % b_width)*b_width_block + x;
			if(x >= width){
				continue;
			}
			sum += entropyTable[in_bytes[(y_pos * width + x_pos)*3 + offset]];
		}
	}
	return sum;
}

#endif //ENTROPY_ESTIMATION
