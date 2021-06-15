#ifndef COLOUR_UNFILTERS_HEADER
#define COLOUR_UNFILTERS_HEADER

#include "numerics.hpp"
#include "2dutils.hpp"
#include "filter_utils.hpp"
#include "colour_filter_utils.hpp"

void colour_unfilter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	for(size_t i=1;i<width;i++){
		in_bytes[i*3 + 0] = add_mod(in_bytes[i*3 + 0],in_bytes[(i - 1)*3 + 0],range);
		in_bytes[i*3 + 1] = add_mod(in_bytes[i*3 + 1],in_bytes[(i - 1)*3 + 1],range);
		in_bytes[i*3 + 2] = add_mod(in_bytes[i*3 + 2],in_bytes[(i - 1)*3 + 2],range);
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width * 3 + 0] = add_mod(in_bytes[y * width * 3 + 0],in_bytes[(y-1) * width * 3 + 0],range);
		in_bytes[y * width * 3 + 1] = add_mod(in_bytes[y * width * 3 + 1],in_bytes[(y-1) * width * 3 + 1],range);
		in_bytes[y * width * 3 + 2] = add_mod(in_bytes[y * width * 3 + 2],in_bytes[(y-1) * width * 3 + 2],range);
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
			in_bytes[((y * width) + i)*3 + 1] = add_mod(
				in_bytes[(y * width + i)*3 + 1],
				ffv1(
					in_bytes[(y * width + i - 1)*3 + 1],
					in_bytes[((y-1) * width + i)*3 + 1],
					in_bytes[((y-1) * width + i - 1)*3 + 1]
				),
				range
			);
			in_bytes[((y * width) + i)*3 + 2] = add_mod(
				in_bytes[(y * width + i)*3 + 2],
				ffv1(
					in_bytes[(y * width + i - 1)*3 + 2],
					in_bytes[((y-1) * width + i)*3 + 2],
					in_bytes[((y-1) * width + i - 1)*3 + 2]
				),
				range
			);
		}
	}
}

void colour_unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint16_t predictor
){
	if(predictor == 0){
		colour_unfilter_all_ffv1(in_bytes, range, width, height);
		return;
	}
	int a = (predictor & 0b1111000000000000) >> 12;
	int b = (predictor & 0b0000111100000000) >> 8;
	int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
	int d = (predictor & 0b0000000000001111);

	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	for(size_t i=1;i<width;i++){
		in_bytes[i*3 + 0] = add_mod(in_bytes[i*3 + 0],in_bytes[(i - 1)*3 + 0],range);
		in_bytes[i*3 + 1] = add_mod(in_bytes[i*3 + 1],in_bytes[(i - 1)*3 + 1],range);
		in_bytes[i*3 + 2] = add_mod(in_bytes[i*3 + 2],in_bytes[(i - 1)*3 + 2],range);
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width * 3 + 0] = add_mod(in_bytes[y * width * 3 + 0],in_bytes[(y-1) * width * 3 + 0],range);
		in_bytes[y * width * 3 + 1] = add_mod(in_bytes[y * width * 3 + 1],in_bytes[(y-1) * width * 3 + 1],range);
		in_bytes[y * width * 3 + 2] = add_mod(in_bytes[y * width * 3 + 2],in_bytes[(y-1) * width * 3 + 2],range);
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[(y * width + i - 1)*3];
			uint8_t TL = in_bytes[((y-1) * width + i - 1)*3];
			uint8_t T = in_bytes[((y-1) * width + i)*3];
			uint8_t TR = in_bytes[((y-1) * width + i + 1)*3];
			uint8_t here = in_bytes[(y * width + i)*3];

			in_bytes[((y * width) + i)*3] = add_mod(
				here,
				clamp(
					(
						a*L + b*T + c*TL + d*TR + halfsum
					)/sum,
					range
				),
				range
			);

			L = in_bytes[(y * width + i - 1)*3 + 1];
			TL = in_bytes[((y-1) * width + i - 1)*3 + 1];
			T = in_bytes[((y-1) * width + i)*3 + 1];
			TR = in_bytes[((y-1) * width + i + 1)*3 + 1];
			here = in_bytes[(y * width + i)*3 + 1];

			in_bytes[((y * width) + i)*3 + 1] = add_mod(
				here,
				clamp(
					(
						a*L + b*T + c*TL + d*TR + halfsum
					)/sum,
					range
				),
				range
			);

			L = in_bytes[(y * width + i - 1)*3 + 2];
			TL = in_bytes[((y-1) * width + i - 1)*3 + 2];
			T = in_bytes[((y-1) * width + i)*3 + 2];
			TR = in_bytes[((y-1) * width + i + 1)*3 + 2];
			here = in_bytes[(y * width + i)*3 + 2];

			in_bytes[((y * width) + i)*3 + 2] = add_mod(
				here,
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

#endif //COLOUR_UNFILTERS_HEADER
