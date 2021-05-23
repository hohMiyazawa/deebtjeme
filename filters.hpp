#ifndef FILTERS_HEADER
#define FILTERS_HEADER

uint8_t* filter_all_ffv1(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - median3(
					L,
					T,
					L + T - TL
				)
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_median(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - median3(
					in_bytes[y * width + i - 1],
					in_bytes[(y-1) * width + i],
					in_bytes[(y-1) * width + i - 1]
				)
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_left(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[y * width + i - 1];
		}
	}
	return filtered;
}

uint8_t* filter_all_top(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[(y-1) * width + i];
		}
	}
	return filtered;
}

uint8_t* filter_all_generic(uint8_t* in_bytes, uint32_t width, uint32_t height,int a,int b,int c,int d){
	uint8_t* filtered = new uint8_t[width * height];
	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			int L = in_bytes[y * width + i - 1];
			int TL = in_bytes[(y-1) * width + i - 1];
			int T = in_bytes[(y-1) * width + i];
			int TR = in_bytes[(y-1) * width + i + 1];
			filtered[(y * width) + i] = in_bytes[y * width + i] - clamp(
				(
					a*L + b*T + c*TL + d*TR + halfsum
				)/sum
			);
		}
	}
	return filtered;
}

uint8_t is_valid_predictor(uint8_t predictor){
	if(predictor == 0){
		return 1;
	}
	else if(predictor == 6){
		return 1;
	}
	else if(predictor == 7){
		return 1;
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

uint8_t* filter_all(uint8_t* in_bytes, uint32_t width, uint32_t height,uint8_t predictor){
	if(predictor == 6){
		return filter_all_ffv1(in_bytes, width, height);
	}
	else if(predictor == 7){
		return filter_all_median(in_bytes, width, height);
	}
	else if(predictor == 68){
		return filter_all_left(in_bytes, width, height);
	}
	else if(predictor == 20){
		return filter_all_top(in_bytes, width, height);
	}
	else{
		int a = (predictor & 0b11000000) >> 6;
		int b = (predictor & 0b00110000) >> 4;
		int c = (int)((predictor & 0b00001100) >> 2) - 1;
		int d = (predictor & 0b00000011);
		printf("abcd %d %d %d %d\n",a,b,c,d);
		return filter_all_generic(in_bytes, width, height,a,b,c,d);
	}
}
#endif //FILTERS