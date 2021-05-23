#ifndef UNFILTERS_HEADER
#define UNFILTERS_HEADER

#include "numerics.hpp"

void unfilter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	if(range == 256){//8bit overflow allowed
		for(size_t i=1;i<width;i++){
			in_bytes[i] = in_bytes[i] + in_bytes[i - 1];//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			in_bytes[y * width] = in_bytes[y * width] + in_bytes[(y-1) * width];//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				uint8_t L = in_bytes[y * width + i - 1];
				uint8_t TL = in_bytes[(y-1) * width + i - 1];
				uint8_t T = in_bytes[(y-1) * width + i];
				in_bytes[(y * width) + i] = in_bytes[y * width + i] + median3(
					L,
					T,
					L + T - TL
				);
			}
		}
	}
	else{
		for(size_t i=1;i<width;i++){
			in_bytes[i] = add_mod(in_bytes[i],in_bytes[i - 1],range);//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			in_bytes[y * width] = add_mod(in_bytes[y * width],in_bytes[(y-1) * width],range);//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				uint8_t L = in_bytes[y * width + i - 1];
				uint8_t TL = in_bytes[(y-1) * width + i - 1];
				uint8_t T = in_bytes[(y-1) * width + i];
				in_bytes[(y * width) + i] = add_mod(
					in_bytes[y * width + i],
					median3(
						L,
						T,
						L + T - TL
					),
					range
				);
			}
		}
	}
}

#endif //UNFILTERS_HEADER
