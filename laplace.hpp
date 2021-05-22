#ifndef LAPLACE_HEADER
#define LAPLACE_HEADER

#include "symbolstats.hpp"

SymbolStats laplace(uint8_t level){
	size_t scale = (2 << level);
	size_t sub_scale = scale - 1;

	size_t startBase[6] = {21845,9362,4369,2141,1105,636};

	size_t base = startBase[level];
	SymbolStats stats;
	stats.freqs[0] = base;
	for(size_t i=1;i<=128;i++){
		base = (base*sub_scale)/scale;
		if(base == 0){
			stats.freqs[i] = 1;
			stats.freqs[256 - i] = 1;
		}
		else{
			stats.freqs[i] = base;
			stats.freqs[256 - i] = base;
		}
	}
	size_t sum = 0;
	for(size_t i=0;i<256;i++){
		sum += stats.freqs[i];
	}
	stats.freqs[0] += (1 << 16) - sum;
	return stats;
}

#endif // LAPLACE_HEADER
