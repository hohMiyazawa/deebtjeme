#ifndef FILTER_UTILS_HEADER
#define FILTER_UTILS_HEADER

uint8_t median3(uint8_t a, uint8_t b, uint8_t c){
	if(a > b){
		if(b > c){
			return b;
		}
		else if(c > a){
			return a;
		}
		else{
			return c;
		}
	}
	else{
		if(b < c){
			return b;
		}
		else if(c > a){
			return c;
		}
		else{
			return a;
		}
	}
}
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

uint8_t semi_paeth(uint8_t L,uint8_t T,uint8_t TL){
	uint8_t min = L;
	uint8_t max = T;
	if(L > T){
		min = T;
		max = L;
	}
	uint8_t grad;
	if(TL >= max){
		return min;
	}
	if(TL <= min){
		return max;
	}
	if(
		(TL - min)
		< (max - TL)
	){
		return max;
	}
	return min;
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

uint8_t is_valid_predictor(uint8_t predictor){
	if(predictor == 0){
		return 1;//ffv1
	}
	else if(predictor == 6){
		return 1;//median
	}
	else if(predictor == 7){
		return 1;//semi-paeth
	}
	else{
		int a = (predictor & 0b11000000) >> 6;
		int b = (predictor & 0b00110000) >> 4;
		int c = (int)((predictor & 0b00001100) >> 2) - 1;
		int d = (predictor & 0b00000011);
		int sum = a + b + c + d;
		if(sum < 1){
			return 0;
		}
		else if(
			(
				(a == 0 && b == 0 && c == 0)
				&& (d == 2 || d == 3)
			)
			||
			(
				(a == 0 && b == 0 && d == 0)
				&& (c == 2)
			)
			||
			(
				(a == 0 && c == 0 && d == 0)
				&& (b == 2 || b == 3)
			)
			||
			(
				(b == 0 && c == 0 && d == 0)
				&& (a == 2 || a == 3)
			)
		){
		}
		return 1;
	}
}
#endif //FILTER_UTILS_HEADER
