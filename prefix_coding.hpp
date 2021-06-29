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
43		20
44	4194304	21
45		21
*/

struct lz_triple{
	uint8_t backref_x;
	uint8_t backref_y;
	uint8_t matchlen;
	uint8_t future;
	uint32_t val_future;
	uint32_t val_matchlen;
	uint32_t backref_x_bits;
	uint32_t backref_y_bits;
	uint32_t matchlen_bits;
	uint32_t future_bits;
};

uint8_t read_prefixcode(RansState& rans, RansDecSymbol* sym, SymbolStats stats, uint8_t*& fileIndex){
	uint32_t cumFreq = RansDecGet(&rans, 16);
	uint8_t s;
	for(size_t j=0;j<256;j++){
		if(stats.cum_freqs[j + 1] > cumFreq){
			s = j;
			break;
		}
	}
	RansDecAdvanceSymbol(&rans, &fileIndex, &sym[s], 16);
	return s;
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

uint32_t prefix_to_val(
	uint8_t prefix,
	RansState& rans,
	uint8_t*& fileIndex,
	RansDecSymbol decode_binary_zero,
	RansDecSymbol decode_binary_one
){
	if(prefix < 4){
		return (uint32_t)prefix;
	}
	uint32_t value = (1 << (prefix/2));
	if(prefix % 2){
		value += (value >> 1);
	}
	uint8_t extrabits = extrabits_from_prefix(prefix);
	for(size_t shift = 0;shift < extrabits;shift++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		if(cumFreq < (1 << 15)){
			RansDecAdvanceSymbol(&rans, &fileIndex, &decode_binary_zero, 16);
		}
		else{
			RansDecAdvanceSymbol(&rans, &fileIndex, &decode_binary_one, 16);
			value += (1 << (extrabits - shift - 1));
		}
	}
	return value;
}

uint32_t prefix_to_val_noExtra(
	uint8_t prefix
){
	if(prefix < 4){
		return (uint32_t)prefix;
	}
	uint32_t value = (1 << (prefix/2));
	if(prefix % 2){
		value += (value >> 1);
	}
	return value;
}

#endif //PREFIX_CODING
