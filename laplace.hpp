#ifndef LAPLACE_HEADER
#define LAPLACE_HEADER

#include "symbolstats.hpp"

SymbolStats laplace(uint8_t level,size_t range){

	uint32_t base = 0b00000000111111111111111111111111;

	SymbolStats stats;
	stats.freqs[0] = base;
	for(size_t i=1;i<256;i++){
		stats.freqs[i] = 0;
	}
	for(size_t i=1;i<=((range + 1) >> 1);i++){
		base = (uint32_t)(((size_t)base * (size_t)level)/(level + 1));
		if(base == 0){
			stats.freqs[i] = 1;
			stats.freqs[range - i] = 1;
		}
		else{
			stats.freqs[i] = base;
			stats.freqs[range - i] = base;
		}
	}
	stats.normalize_freqs(1 << 16);
	return stats;
}

#endif // LAPLACE_HEADER
