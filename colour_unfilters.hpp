#ifndef COLOUR_UNFILTERS_HEADER
#define COLOUR_UNFILTERS_HEADER

#include "numerics.hpp"
#include "2dutils.hpp"


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

#endif //COLOUR_UNFILTERS_HEADER
