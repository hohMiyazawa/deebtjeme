#ifndef PREFIX_CODING_HEADER
#define PREFIX_CODING_HEADER

#include "rans_byte.h"
#include "symbolstats.hpp"

uint8_t read_prefixcode(RansState* r, RansDecSymbol* sym, SymbolStats stats, uint8_t** fileIndex){
	return 0;//todo
}

size_t prefix_to_val(uint8_t prefix, uint8_t& lz_bit_buffer, uint8_t& lz_bit_buffer_index, uint8_t** fileIndex){
	return 0;//todo
}

uint8_t inverse_prefix(size_t value){
	return 0;//todo
}

#endif //PREFIX_CODING
