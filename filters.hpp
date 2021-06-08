#ifndef FILTERS_HEADER
#define FILTERS_HEADER

#include "numerics.hpp"

uint8_t* filter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];
	filtered[0] = in_bytes[0];//TL prediction
	if(range == 256){
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
					in_bytes[y * width + i] - ffv1(
						L,
						T,
						TL
					)
				);
			}
		}
	}
	else{
		for(size_t i=1;i<width;i++){
			filtered[i] = sub_mod(in_bytes[i],in_bytes[i - 1],range);//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			filtered[y * width] = sub_mod(in_bytes[y * width],in_bytes[(y-1) * width],range);//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				uint8_t L = in_bytes[y * width + i - 1];
				uint8_t TL = in_bytes[(y-1) * width + i - 1];
				uint8_t T = in_bytes[(y-1) * width + i];
				filtered[(y * width) + i] = sub_mod(
					in_bytes[y * width + i],
					ffv1(
						L,
						T,
						TL
					),
					range
				);
			}
		}
	}
	return filtered;
}

uint8_t* filter_all_left(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	if(range == 256){
		for(size_t i=1;i<width;i++){
			filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[y * width + i - 1];
			}
		}
	}
	else{
		for(size_t i=1;i<width;i++){
			filtered[i] = sub_mod(in_bytes[i],in_bytes[i - 1],range);//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			filtered[y * width] = sub_mod(
				in_bytes[y * width],
				in_bytes[(y-1) * width],
				range
			);//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				filtered[(y * width) + i] = sub_mod(
					in_bytes[y * width + i],
					in_bytes[y * width + i - 1],
					range
				);
			}
		}
	}
	return filtered;
}

uint8_t* filter_all_top(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	if(range == 256){
		for(size_t i=1;i<width;i++){
			filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			for(size_t i=0;i<width;i++){
				filtered[(y * width) + i] = in_bytes[y * width + i] - in_bytes[(y-1) * width + i];
			}
		}
	}
	else{
		for(size_t i=1;i<width;i++){
			filtered[i] = sub_mod(in_bytes[i],in_bytes[i - 1],range);//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			for(size_t i=0;i<width;i++){
				filtered[(y * width) + i] = sub_mod(
					in_bytes[y * width + i],
					in_bytes[(y-1) * width + i],
					range
				);
			}
		}
	}
	return filtered;
}

uint8_t* filter_all_generic_noD(uint8_t* in_bytes, uint32_t width, uint32_t height,int a,int b,int c){
	uint8_t* filtered = new uint8_t[width * height];
	uint8_t sum = a + b + c;
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
			filtered[(y * width) + i] = in_bytes[y * width + i] - clamp(
				(
					a*L + b*T + c*TL + halfsum
				)/sum
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_generic(uint8_t* in_bytes, uint32_t width, uint32_t height,int a,int b,int c,int d){
	if(d == 0){
		return filter_all_generic_noD(in_bytes, width, height, a, b, c);
	}
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

uint8_t* filter_all(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height,uint16_t predictor){
	if(predictor == 0){
		return filter_all_ffv1(in_bytes, range, width, height);
	}
	else{
		int a = (predictor & 0b1111000000000000) >> 12;
		int b = (predictor & 0b0000111100000000) >> 8;
		int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
		int d = (predictor & 0b0000000000001111);
		//printf("abcd %d %d %d %d\n",a,b,c,d);
		return filter_all_generic(in_bytes, width, height,a,b,c,d);
	}
}
#endif //FILTERS
