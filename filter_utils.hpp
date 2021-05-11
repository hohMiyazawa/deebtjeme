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
#endif //FILTER_UTILS_HEADER
