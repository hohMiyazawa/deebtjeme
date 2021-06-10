#ifndef COLOUR_UNFILTERS_HEADER
#define COLOUR_UNFILTERS_HEADER

#include "numerics.hpp"
#include "2dutils.hpp"
#include "colour_filter_utils.hpp"


void colourSub_unfilter_all_left(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	in_bytes[1] = add_mod(in_bytes[1],in_bytes[0],range);
	in_bytes[2] = add_mod(in_bytes[2],in_bytes[0],range);

	for(size_t i=1;i<width;i++){
		in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		int16_t r_L = (int16_t)in_bytes[(i - 1)*3 + 1] - (int16_t)in_bytes[(i - 1)*3];
		in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + r_L + in_bytes[i*3] + range) % range;
		int16_t b_L = (int16_t)in_bytes[(i - 1)*3 + 2] - (int16_t)in_bytes[(i - 1)*3];
		in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + b_L + in_bytes[i*3] + range) % range;
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width * 3] = add_mod(in_bytes[y * width * 3],in_bytes[(y-1) * width * 3],range);
		int16_t r_T = (int16_t)in_bytes[(y-1) * width * 3 + 1] - (int16_t)in_bytes[(y-1) * width * 3];
		in_bytes[y * width * 3 + 1] = (in_bytes[y * width * 3 + 1] + r_T + in_bytes[y * width * 3] + range) % range;
		int16_t b_T = (int16_t)in_bytes[(y-1) * width * 3 + 2] - (int16_t)in_bytes[(y-1) * width * 3];
		in_bytes[y * width * 3 + 2] = (in_bytes[y * width * 3 + 2] + b_T + in_bytes[y * width * 3] + range) % range;
		for(size_t i=1;i<width;i++){
			in_bytes[((y * width) + i)*3] = add_mod(
				in_bytes[(y * width + i)*3],
				in_bytes[(y * width + i - 1)*3],
				range
			);
			int16_t r_L = (int16_t)in_bytes[(y * width + i - 1)*3 + 1] - (int16_t)in_bytes[(y * width + i - 1)*3];
			in_bytes[((y * width) + i)*3 + 1] = (in_bytes[((y * width) + i)*3 + 1] + r_L + in_bytes[((y * width) + i)*3] + range) % range;
			int16_t b_L = (int16_t)in_bytes[(y * width + i - 1)*3 + 2] - (int16_t)in_bytes[(y * width + i - 1)*3];
			in_bytes[((y * width) + i)*3 + 2] = (in_bytes[((y * width) + i)*3 + 2] + b_L + in_bytes[((y * width) + i)*3] + range) % range;
		}
	}
}

void colourSub_unfilter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	in_bytes[1] = add_mod(in_bytes[1],in_bytes[0],range);
	in_bytes[2] = add_mod(in_bytes[2],in_bytes[0],range);

	for(size_t i=1;i<width;i++){
		in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		int16_t r_L = (int16_t)in_bytes[(i - 1)*3 + 1] - (int16_t)in_bytes[(i - 1)*3];
		in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + r_L + in_bytes[i*3] + range) % range;
		int16_t b_L = (int16_t)in_bytes[(i - 1)*3 + 2] - (int16_t)in_bytes[(i - 1)*3];
		in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + b_L + in_bytes[i*3] + range) % range;
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width * 3] = add_mod(in_bytes[y * width * 3],in_bytes[(y-1) * width * 3],range);
		int16_t r_T = (int16_t)in_bytes[(y-1) * width * 3 + 1] - (int16_t)in_bytes[(y-1) * width * 3];
		in_bytes[y * width * 3 + 1] = (in_bytes[y * width * 3 + 1] + r_T + in_bytes[y * width * 3] + range) % range;
		int16_t b_T = (int16_t)in_bytes[(y-1) * width * 3 + 2] - (int16_t)in_bytes[(y-1) * width * 3];
		in_bytes[y * width * 3 + 2] = (in_bytes[y * width * 3 + 2] + b_T + in_bytes[y * width * 3] + range) % range;
		for(size_t i=1;i<width;i++){
			in_bytes[((y * width) + i)*3] = add_mod(
				in_bytes[(y * width + i)*3],
				ffv1(
					in_bytes[(y * width + i - 1)*3],
					in_bytes[((y-1) * width + i)*3],
					in_bytes[((y-1) * width + i - 1)*3]
				),
				range
			);
			int16_t r_L = (int16_t)in_bytes[(y * width + i - 1)*3 + 1] - (int16_t)in_bytes[(y * width + i - 1)*3];
			int16_t r_T = (int16_t)in_bytes[((y-1) * width + i)*3 + 1] - (int16_t)in_bytes[((y-1) * width + i)*3];
			int16_t r_TL= (int16_t)in_bytes[((y-1) * width + i - 1)*3 + 1] - (int16_t)in_bytes[((y-1) * width + i - 1)*3];
			in_bytes[((y * width) + i)*3 + 1] = (in_bytes[((y * width) + i)*3 + 1] + ffv1(r_L,r_T,r_TL) + in_bytes[((y * width) + i)*3] + range) % range;
			int16_t b_L = (int16_t)in_bytes[(y * width + i - 1)*3 + 2] - (int16_t)in_bytes[(y * width + i - 1)*3];
			int16_t b_T = (int16_t)in_bytes[((y-1) * width + i)*3 + 2] - (int16_t)in_bytes[((y-1) * width + i)*3];
			int16_t b_TL= (int16_t)in_bytes[((y-1) * width + i - 1)*3 + 2] - (int16_t)in_bytes[((y-1) * width + i - 1)*3];
			in_bytes[((y * width) + i)*3 + 2] = (in_bytes[((y * width) + i)*3 + 2] + ffv1(b_L,b_T,b_TL) + in_bytes[((y * width) + i)*3] + range) % range;
		}
	}
}

void colourSub_unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint16_t predictor
){
	if(predictor == 0){
		colourSub_unfilter_all_ffv1(in_bytes, range, width, height);
	}
	else if(predictor == 0b0001000011010000){
		colourSub_unfilter_all_left(in_bytes, range, width, height);
	}
	else{
		//figure out later
/*
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
*/
	}
}

#endif //COLOUR_UNFILTERS_HEADER
