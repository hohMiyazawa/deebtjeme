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
	uint32_t val_backref;
	uint32_t val_matchlen;
	uint32_t val_future;
};

struct lz_triple_c{
	uint8_t backref;
	uint8_t matchlen;
	uint8_t future;
	uint32_t backref_bits;
	uint32_t matchlen_bits;
	uint32_t future_bits;
};

int8_t lut_x[120] = {
 0,   1,   1,  -1,   0,   2,   1,  -1, 
 2,  -2,   2,  -2,   0,   3,   1,  -1, 
 3,  -3,   2,  -2,   3,  -3,   0,   4, 
 1,  -1,   4,  -4,   3,  -3,   2,  -2, 
 4,  -4,   0,   3,  -3,   4,  -4,   5, 
 1,  -1,   5,  -5,   2,  -2,   5,  -5, 
 4,  -4,   3,  -3,   5,  -5,   0,   6, 
 1,  -1,   6,  -6,   2,  -2,   6,  -6, 
 4,  -4,   5,  -5,   3,  -3,   6,  -6, 
 0,   7,   1,  -1,   5,  -5,   7,  -7, 
 4,  -4,   6,  -6,   2,  -2,   7,  -7, 
 3,  -3,   7,  -7,   5,  -5,   6,  -6, 
 8,   4,  -4,   7,  -7,   8,   8,   6, 
-6,   8,   5,  -5,   7,  -7,   8,   6, 
-6,   7,  -7,   8,   7,  -7,   8,   8};

int8_t lut_y[120] = {
 1,   0,   1,   1,   2,   0,   2,   2,
 1,   1,   2,   2,   3,   0,   3,   3,
 1,   1,   3,   3,   2,   2,   4,   0,
 4,   4,   1,   1,   3,   3,   4,   4,
 2,   2,   5,   4,   4,   3,   3,   0,
 5,   5,   1,   1,   5,   5,   2,   2,
 4,   4,   5,   5,   3,   3,   6,   0,
 6,   6,   1,   1,   6,   6,   2,   2,
 5,   5,   4,   4,   6,   6,   3,   3,
 7,   0,   7,   7,   5,   5,   1,   1,
 6,   6,   4,   4,   7,   7,   2,   2,
 7,   7,   3,   3,   6,   6,   5,   5,
 0,   7,   7,   4,   4,   1,   2,   6,
 6,   3,   7,   7,   5,   5,   4,   7,
 7,   6,   6,   5,   7,   7,   6,   7};

uint8_t reverse_lut[120] = {
119,116,111,106, 97, 88, 84, 74, 72, 75, 85, 89, 98,107,112,117,
118,113,103, 92, 80, 68, 60, 56, 54, 57, 61, 69, 81, 93,104,114,
115,108, 94, 76, 64, 50, 44, 40, 34, 41, 45, 51, 65, 77, 95,109,
110, 99, 82, 66, 48, 35, 30, 24, 22, 25, 31, 36, 49, 67, 83,100,
105, 90, 70, 52, 37, 28, 18, 14, 12, 15, 19, 29, 38, 53, 71, 91,
102, 86, 62, 46, 32, 20, 10,  6,  4,  7, 11, 21, 33, 47, 63, 87,
101, 78, 58, 42, 26, 16,  8,  2,  0,  3,  9, 17, 27, 43, 59, 79,
 96, 73, 55, 39, 23, 13,  5,  1
};



uint8_t read_prefixcode(RansState& rans, RansDecSymbol* sym, SymbolStats stats, uint8_t*& fileIndex){
	uint32_t cumFreq = RansDecGet(&rans, 16);
	uint8_t s = 0;
	for(size_t j=0;j<256;j++){
		if(stats.cum_freqs[j + 1] > cumFreq){
			s = j;
			break;
		}
	}
	RansDecAdvanceSymbol(&rans, &fileIndex, &sym[s], 16);
	return s;
}
/*
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
*/

uint8_t native_inverse_prefix(size_t value){
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

uint8_t native_extrabits_from_prefix(uint8_t prefix){
	if(prefix < 4){
		return 0;
	}
	else{
		return prefix/2 - 1;
	}
}

uint32_t native_prefix_extrabits(size_t value){
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

uint32_t native_prefix_to_val(
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
	uint8_t extrabits = native_extrabits_from_prefix(prefix);
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

size_t extra_offset = 4;

uint8_t inverse_prefix(size_t value){
	if(value < extra_offset){
		return (uint8_t)value;
	}
	else{
		return extra_offset + native_inverse_prefix(value - extra_offset);
	}
}

uint8_t extrabits_from_prefix(uint8_t prefix){
	if(prefix < extra_offset){
		return 0;
	}
	else{
		return native_extrabits_from_prefix(prefix - extra_offset);
	}
}

uint32_t prefix_extrabits(size_t value){
	if(value < extra_offset){
		return 0;
	}
	else{
		return native_prefix_extrabits(value - extra_offset);
	}
}

uint32_t prefix_to_val(
	uint8_t prefix,
	RansState& rans,
	uint8_t*& fileIndex,
	RansDecSymbol decode_binary_zero,
	RansDecSymbol decode_binary_one
){
	if(prefix < extra_offset){
		return (uint32_t)prefix;
	}
	else{
		return extra_offset + native_prefix_to_val(
			prefix - extra_offset,
			rans,
			fileIndex,
			decode_binary_zero,
			decode_binary_one
		);
	}
}

uint8_t read32(
	RansState& rans,
	uint8_t*& fileIndex,
	RansDecSymbol decode_binary_zero,
	RansDecSymbol decode_binary_one
){
	uint8_t value = 0;
	for(size_t shift = 0;shift < 5;shift++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		if(cumFreq < (1 << 15)){
			RansDecAdvanceSymbol(&rans, &fileIndex, &decode_binary_zero, 16);
		}
		else{
			RansDecAdvanceSymbol(&rans, &fileIndex, &decode_binary_one, 16);
			value += (1 << (4 - shift));
		}
	}
	return value;
}

#endif //PREFIX_CODING
