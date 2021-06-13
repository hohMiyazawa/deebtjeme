#ifndef COLOUR_FILTERS_HEADER
#define COLOUR_FILTERS_HEADER

#include "numerics.hpp"
#include "colour_filter_utils.hpp"

uint8_t* colourSub_filter_all_left(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height * 3];

	filtered[0] = in_bytes[0];
	filtered[1] = (uint8_t)(((int)in_bytes[1] - (int)in_bytes[0] + range) % range);
	filtered[2] = (uint8_t)(((int)in_bytes[2] - (int)in_bytes[0] + range) % range);

	for(size_t i=1;i<width;i++){
		filtered[i*3] = sub_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		filtered[i*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 1] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 1] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
		filtered[i*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 2] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 2] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width*3] = sub_mod(in_bytes[y*width*3],in_bytes[(y - 1)*width*3],range);
		filtered[y*width*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 1] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 1] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		filtered[y*width*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 2] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 2] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		for(size_t i=1;i<width;i++){
			filtered[(y*width + i)*3] = sub_mod(in_bytes[(y*width + i)*3],in_bytes[(y*width + i - 1)*3],range);
			int16_t r_L = (int16_t)in_bytes[(y * width + i - 1)*3 + 1] - (int16_t)in_bytes[(y * width + i - 1)*3];
			int16_t r_here = (int16_t)in_bytes[(y * width + i)*3 + 1] - (int16_t)in_bytes[(y * width + i)*3];
			filtered[(y*width + i)*3 + 1] = (
				r_here - r_L
				+ 2*range
			) % range;
			filtered[(y*width + i)*3 + 2] = (uint8_t)(
				(
					
					(
						((int)in_bytes[(y*width + i)*3 + 2] - (int)in_bytes[(y*width + i)*3])
						- ((int)in_bytes[(y*width + i - 1)*3 + 2] - (int)in_bytes[(y*width + i - 1)*3])
					)
					+ 2*range
				) % range
			);
		}
	}

	return filtered;
}

uint8_t* colourSub_filter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height * 3];

	filtered[0] = in_bytes[0];
	filtered[1] = (uint8_t)(((int)in_bytes[1] - (int)in_bytes[0] + range) % range);
	filtered[2] = (uint8_t)(((int)in_bytes[2] - (int)in_bytes[0] + range) % range);

	for(size_t i=1;i<width;i++){
		filtered[i*3] = sub_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		filtered[i*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 1] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 1] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
		filtered[i*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 2] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 2] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width*3] = sub_mod(in_bytes[y*width*3],in_bytes[(y - 1)*width*3],range);
		filtered[y*width*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 1] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 1] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		filtered[y*width*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 2] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 2] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		for(size_t i=1;i<width;i++){
			uint8_t L  = in_bytes[(y * width + i - 1)*3];
			uint8_t T  = in_bytes[((y-1) * width + i)*3];
			uint8_t TL = in_bytes[((y-1) * width + i - 1)*3];
			filtered[((y * width) + i)*3] = sub_mod(
				in_bytes[((y * width) + i)*3],
				ffv1(
					L,
					T,
					TL
				),
				range
			);
			int16_t r_L =  (int)in_bytes[(y * width + i - 1)*3 + 1] - (int)in_bytes[(y * width + i - 1)*3];
			int16_t r_TL = (int)in_bytes[((y-1) * width + i - 1)*3 + 1] - (int)in_bytes[((y-1) * width + i - 1)*3];
			int16_t r_T =  (int)in_bytes[((y-1) * width + i)*3 + 1] - (int)in_bytes[((y-1) * width + i)*3];
			int16_t r_here=(int)in_bytes[(y * width + i)*3 + 1] - (int)in_bytes[(y * width + i)*3];
			filtered[((y * width) + i)*3 + 1] = (uint8_t)(
				(
					
					r_here - ffv1(r_L,r_T,r_TL)
					+ 2*range
				) % range
			);
			int16_t b_L =  (int)in_bytes[(y * width + i - 1)*3 + 2] - (int)in_bytes[(y * width + i - 1)*3];
			int16_t b_TL = (int)in_bytes[((y-1) * width + i - 1)*3 + 2] - (int)in_bytes[((y-1) * width + i - 1)*3];
			int16_t b_T =  (int)in_bytes[((y-1) * width + i)*3 + 2] - (int)in_bytes[((y-1) * width + i)*3];
			int16_t b_here=(int)in_bytes[(y * width + i)*3 + 2] - (int)in_bytes[(y * width + i)*3];
			filtered[((y * width) + i)*3 + 2] = (uint8_t)(
				(
					
					b_here - ffv1(b_L,b_T,b_TL)
					+ 2*range
				) % range
			);
		}
	}

	return filtered;
}

uint8_t* colourSub_filter_all_generic(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height, int a, int b, int c, int d){
	uint8_t* filtered = new uint8_t[width * height * 3];

	filtered[0] = in_bytes[0];
	filtered[1] = (uint8_t)(((int)in_bytes[1] - (int)in_bytes[0] + range) % range);
	filtered[2] = (uint8_t)(((int)in_bytes[2] - (int)in_bytes[0] + range) % range);

	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	for(size_t i=1;i<width;i++){
		filtered[i*3] = sub_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		filtered[i*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 1] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 1] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
		filtered[i*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[i*3 + 2] - (int)in_bytes[i*3])
					- ((int)in_bytes[(i-1)*3 + 2] - (int)in_bytes[(i-1)*3])
				)
				+ 2*range
			) % range
		);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width*3] = sub_mod(in_bytes[y*width*3],in_bytes[(y - 1)*width*3],range);
		filtered[y*width*3 + 1] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 1] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 1] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		filtered[y*width*3 + 2] = (uint8_t)(
			(
				
				(
					((int)in_bytes[y*width*3 + 2] - (int)in_bytes[y*width*3])
					- ((int)in_bytes[(y-1)*width*3 + 2] - (int)in_bytes[(y-1)*width*3])
				)
				+ 2*range
			) % range
		);
		for(size_t i=1;i<width;i++){
			uint8_t L  = in_bytes[(y * width + i - 1)*3];
			uint8_t T  = in_bytes[((y-1) * width + i)*3];
			uint8_t TL = in_bytes[((y-1) * width + i - 1)*3];
			uint8_t TR = in_bytes[((y-1) * width + i + 1)*3];
			filtered[((y * width) + i)*3] = (in_bytes[(y * width + i)*3] - clamp(
				(
					a*L + b*T + c*TL + d*TR + halfsum
				)/sum,
				range
			) + range) % range;
			int16_t r_L =  (int)in_bytes[(y * width + i - 1)*3 + 1] - (int)in_bytes[(y * width + i - 1)*3];
			int16_t r_T =  (int)in_bytes[((y-1) * width + i)*3 + 1] - (int)in_bytes[((y-1) * width + i)*3];
			int16_t r_TL = (int)in_bytes[((y-1) * width + i - 1)*3 + 1] - (int)in_bytes[((y-1) * width + i - 1)*3];
			int16_t r_TR = (int)in_bytes[((y-1) * width + i + 1)*3 + 1] - (int)in_bytes[((y-1) * width + i + 1)*3];
			int16_t r_here=(int)in_bytes[(y * width + i)*3 + 1] - (int)in_bytes[(y * width + i)*3];
			filtered[((y * width) + i)*3 + 1] = (uint8_t)(
				(
					
					r_here - i_clamp(
						roundDownDivide(
							a*r_L + b*r_T + c*r_TL + d*r_TR + halfsum,
							sum
						),
						-range,
						range
					)
					+ 2*range
				) % range
			);
			int16_t b_L =  (int)in_bytes[(y * width + i - 1)*3 + 2] - (int)in_bytes[(y * width + i - 1)*3];
			int16_t b_T =  (int)in_bytes[((y-1) * width + i)*3 + 2] - (int)in_bytes[((y-1) * width + i)*3];
			int16_t b_TL = (int)in_bytes[((y-1) * width + i - 1)*3 + 2] - (int)in_bytes[((y-1) * width + i - 1)*3];
			int16_t b_TR = (int)in_bytes[((y-1) * width + i + 1)*3 + 2] - (int)in_bytes[((y-1) * width + i + 1)*3];
			int16_t b_here=(int)in_bytes[(y * width + i)*3 + 2] - (int)in_bytes[(y * width + i)*3];
			filtered[((y * width) + i)*3 + 2] = (uint8_t)(
				(
					
					b_here - i_clamp(
						roundDownDivide(
							a*b_L + b*b_T + c*b_TL + d*b_TR + halfsum,
							sum
						),
						-range,
						range
					)
					+ 2*range
				) % range
			);
		}
	}

	return filtered;
}

uint8_t* colourSub_filter_all(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height,uint16_t predictor){
	if(predictor == 0){
		return colourSub_filter_all_ffv1(in_bytes, range, width, height);
	}
	else if(predictor == 0b0001000011010000){
		return colourSub_filter_all_left(in_bytes, range, width, height);
	}
	else{
		int a = (predictor & 0b1111000000000000) >> 12;
		int b = (predictor & 0b0000111100000000) >> 8;
		int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
		int d = (predictor & 0b0000000000001111);
		//printf("abcd %d %d %d %d\n",a,b,c,d);
		return colourSub_filter_all_generic(in_bytes, range, width, height,a,b,c,d);
	}
}

#endif //COLOUR_FILTERS_HEADER
