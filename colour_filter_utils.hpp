#ifndef COLOUR_FILTER_UTILS_HEADER
#define COLOUR_FILTER_UTILS_HEADER

int16_t ffv1(int16_t L,int16_t T,int16_t TL){
	int16_t min = L;
	int16_t max = T;
	if(L > T){
		min = T;
		max = L;
	}
	if(TL >= max){
		return min;
	}
	if(TL <= min){
		return max;
	}
	return max - (TL - min);
}

uint16_t ffv1(uint16_t L,uint16_t T,uint16_t TL){
	uint16_t min = L;
	uint16_t max = T;
	if(L > T){
		min = T;
		max = L;
	}
	if(TL >= max){
		return min;
	}
	if(TL <= min){
		return max;
	}
	return max - (TL - min);
}

uint8_t i_clamp(int16_t a,int16_t lower,int16_t upper){
	if(a < lower){
		return lower;
	}
	else if(a >= upper){
		return upper - 1;
	}
	else{
		return (int16_t)a;
	}
}

#endif //COLOUR_FILTER_UTILS_HEADER
