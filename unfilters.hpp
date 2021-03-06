#ifndef UNFILTERS_HEADER
#define UNFILTERS_HEADER

#include "numerics.hpp"
#include "2dutils.hpp"
#include "filter_utils.hpp"

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
				in_bytes[(y * width) + i] = in_bytes[y * width + i] + ffv1(
					L,
					T,
					TL
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
}

void unfilter_all_left(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	if(range == 256){//8bit overflow allowed
		for(size_t i=1;i<width;i++){
			in_bytes[i] = in_bytes[i] + in_bytes[i - 1];//top edge is always left-predicted
		}
		for(size_t y=1;y<height;y++){
			in_bytes[y * width] = in_bytes[y * width] + in_bytes[(y-1) * width];//left edge is always top-predicted
			for(size_t i=1;i<width;i++){
				in_bytes[(y * width) + i] = in_bytes[y * width + i] + in_bytes[y * width + i - 1];
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
				in_bytes[(y * width) + i] = add_mod(
					in_bytes[y * width + i],
					in_bytes[y * width + i - 1],
					range
				);
			}
		}
	}
}

void unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint16_t* predictor_image,
	uint32_t predictor_width,
	uint32_t predictor_height
){
	if(predictor_width*predictor_height == 1){
		if(predictor_image[0] == 0){
			unfilter_all_ffv1(in_bytes, range, width, height);
			return;
		}
	}
	uint32_t predictor_width_block  = (width + predictor_width - 1)/predictor_width;
	uint32_t predictor_height_block = (height + predictor_height - 1)/predictor_height;
	for(size_t i=1;i<width;i++){
		in_bytes[i] = add_mod(in_bytes[i],in_bytes[i - 1],range);
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width] = add_mod(in_bytes[y * width],in_bytes[(y-1) * width],range);
		for(size_t i=1;i<width;i++){
			uint16_t predictor = predictor_image[predictor_width * (y/predictor_height_block) + i/predictor_width_block];

			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];

			if(predictor == 0){
				in_bytes[(y * width) + i] = add_mod(
					in_bytes[y * width + i],
					ffv1(
						L,
						T,
						TL
					),
					range
				);
			}
			else{
				uint8_t TR = in_bytes[(y-1) * width + i + 1];

				int a = (predictor & 0b1111000000000000) >> 12;
				int b = (predictor & 0b0000111100000000) >> 8;
				int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
				int d = (predictor & 0b0000000000001111);

				uint8_t sum = a + b + c + d;
				uint8_t halfsum = sum >> 1;

				in_bytes[(y * width) + i] = add_mod(
					in_bytes[(y * width) + i],
					clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}
	}
}

void unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint16_t predictor
){
	if(predictor == 0){
		unfilter_all_ffv1(in_bytes, range, width, height);
	}
	else if(predictor == 0b0001000011010000){
		unfilter_all_left(in_bytes, range, width, height);
	}
	else{
		for(size_t i=1;i<width;i++){
			in_bytes[i] = add_mod(in_bytes[i],in_bytes[i - 1],range);
		}
		for(size_t y=1;y<height;y++){
			in_bytes[y * width] = add_mod(in_bytes[y * width],in_bytes[(y-1) * width],range);
			for(size_t i=1;i<width;i++){

				uint8_t L = in_bytes[y * width + i - 1];
				uint8_t TL = in_bytes[(y-1) * width + i - 1];
				uint8_t T = in_bytes[(y-1) * width + i];
				uint8_t TR = in_bytes[(y-1) * width + i + 1];

				int a = (predictor & 0b1111000000000000) >> 12;
				int b = (predictor & 0b0000111100000000) >> 8;
				int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
				int d = (predictor & 0b0000000000001111);

				uint8_t sum = a + b + c + d;
				uint8_t halfsum = sum >> 1;

				in_bytes[(y * width) + i] = add_mod(
					in_bytes[(y * width) + i],
					clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}
	}
}

#endif //UNFILTERS_HEADER
