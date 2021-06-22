#ifndef PREFIX_CODING_HEADER
#define PREFIX_CODING_HEADER

#include "rans_byte.h"
#include "symbolstats.hpp"
#include "numerics.hpp"

/*
0	0
1	1
2	2
3	3
4	4-5	1	100-101		6
5	6-7	1	110-111		6
6	8-11	2	1000-1011	8
7	12-15	2	1100-1111	8
8	16-23	3	10000-10111	10
9	24-31	3	11000-11111	10
10	32-47	4	100000-101111	12
11	48-63	4	110000-111111	12
12	64	5
13	96	5
14	128	6
15	192	6
16	256	7
17	384	7
18	512	8
19		8
20	1024	9
21		9
22	2048	10
23		10
24	4096	11
25		11
26	8192	12
27		12
28	16384	13
29		13
30	32768	14
31		14
32	65536	15
33		15
34	131072	16
35		16
36	262144	17
37		17
38	524288	18
39		18
40	1048576	19
41		19
42	2097152	20
*/

struct lz_triple{
	uint8_t backref_x;
	uint8_t backref_y;
	uint8_t matchlen;
	uint8_t future;
	uint32_t backref_x_bits;
	uint32_t backref_y_bits;
	uint32_t matchlen_bits;
	uint32_t future_bits;
};

uint8_t read_prefixcode(RansState* r, RansDecSymbol* sym, SymbolStats stats, uint8_t** fileIndex){
	return 0;//todo
}

size_t prefix_to_val(uint8_t prefix, uint8_t& lz_bit_buffer, uint8_t& lz_bit_buffer_index, uint8_t** fileIndex){
	return 0;//todo
}

uint8_t inverse_prefix(size_t value){
	if(value < 5){
		return (uint8_t)value;
	}
	uint8_t magnitude  = log2_plus(value);
	uint8_t magnitude2 = log2_plus(value - (1 << (magnitude - 1)));
	uint8_t halfcode = magnitude*2;
	if(magnitude2 + 1 == magnitude){
		return halfcode - 1;
	}
	else{
		return halfcode - 2;
	}
}

uint8_t extrabits_from_prefix(uint8_t prefix){
	if(prefix < 4){
		return 0;
	}
	else{
		return prefix/2 - 1;
	}
}

uint32_t prefix_extrabits(size_t value){
	if(value < 4){
		return 0;
	}
	uint8_t magnitude  = log2_plus(value);
	uint8_t magnitude2 = log2_plus(value - (1 << (magnitude - 1)));
	if(magnitude2 + 1 == magnitude){
		return value - (1 << (magnitude - 1)) - (1 << (magnitude2 - 1));
	}
	else{
		return value - (1 << (magnitude - 1));
	}
}

#endif //PREFIX_CODING
