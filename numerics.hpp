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

uint8_t add_mod(uint8_t a, uint8_t b, uint32_t modulo){
	if(b == 0 ){
		return a;
	}

	b = modulo - b;
	if(a >= b){
		return a - b;
	}
	else{
		return modulo - b + a;
	}
}

uint8_t sub_mod(uint8_t a, uint8_t b, uint32_t modulo){
	if (a >= b){
		return a - b;
	}
	else{
		return modulo - b + a;
	}
}

#endif // NUMERICS_HEADER
