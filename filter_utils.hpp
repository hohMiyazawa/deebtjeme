#ifndef FILTER_UTILS_HEADER
#define FILTER_UTILS_HEADER

uint8_t ffv1(uint8_t L,uint8_t T,uint8_t TL){
	uint8_t min = L;
	uint8_t max = T;
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

uint8_t clamp(int a){
	if(a < 0){
		return 0;
	}
	else if(a > 255){
		return 255;
	}
	else{
		return (uint8_t)a;
	}
}

uint8_t clamp(int a,uint32_t range){
	if(a < 0){
		return 0;
	}
	else if(a > (range - 1)){
		return (range - 1);
	}
	else{
		return (uint8_t)a;
	}
}

uint8_t clamp(int a,uint8_t lower,uint8_t upper){
	if(a < lower){
		return lower;
	}
	else if(a > upper){
		return upper;
	}
	else{
		return (uint8_t)a;
	}
}

uint8_t is_valid_predictor(uint16_t predictor){
	if(predictor == 0){
		return 1;//ffv1
	}
	else{
		int a = (predictor & 0b1111000000000000) >> 12;
		int b = (predictor & 0b0000111100000000) >> 8;
		int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
		int d = (predictor & 0b0000000000001111);
		int sum = a + b + c + d;
		if(sum < 1){
			return 0;
		}
		else if(
			(a%2 + b%2 + c%2 + d%2) == 0
			|| (a%3 + b%3 + c%3 + d%3) == 0
			|| (a%5 + b%5 + c%5 + d%5) == 0
			|| (a%7 + b%7 + c%7 + d%7) == 0
			|| (a%11 + b%11 + c%11 + d%11) == 0
			|| (a%13 + b%13 + c%13 + d%13) == 0
		){
			return 0;
		}
		return 1;
	}
}
#endif //FILTER_UTILS_HEADER
