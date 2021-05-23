#ifndef NUMERICS_HEADER
#define NUMERICS_HEADER

uint8_t log2_plus(uint32_t input){
	if(!input){
		return 0;
	}
	uint8_t pow = 0;
	while(input){
		input = input >> 1;
		pow++;
	}
	return pow;
}

#endif // NUMERICS_HEADER
