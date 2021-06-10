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

#endif //COLOUR_FILTER_UTILS_HEADER
