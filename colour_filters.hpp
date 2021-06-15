#ifndef COLOUR_FILTERS_HEADER
#define COLOUR_FILTERS_HEADER

#include "numerics.hpp"
#include "colour_filter_utils.hpp"

uint8_t* colour_filter_all_ffv1(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height * 3];

	filtered[0] = in_bytes[0];
	filtered[1] = in_bytes[1];
	filtered[2] = in_bytes[2];

	for(size_t i=1;i<width;i++){
		filtered[i*3] = sub_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		filtered[i*3 + 1] = sub_mod(in_bytes[i*3 + 1],in_bytes[(i - 1)*3 + 1],range);
		filtered[i*3 + 2] = sub_mod(in_bytes[i*3 + 2],in_bytes[(i - 1)*3 + 2],range);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width*3] = sub_mod(in_bytes[y*width*3],in_bytes[(y - 1)*width*3],range);
		filtered[y*width*3 + 1] = sub_mod(in_bytes[y*width*3 + 1],in_bytes[(y - 1)*width*3 + 1],range);
		filtered[y*width*3 + 2] = sub_mod(in_bytes[y*width*3 + 2],in_bytes[(y - 1)*width*3 + 2],range);
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
			L  = in_bytes[(y * width + i - 1)*3 + 1];
			T  = in_bytes[((y-1) * width + i)*3 + 1];
			TL = in_bytes[((y-1) * width + i - 1)*3 + 1];
			filtered[((y * width) + i)*3 + 1] = sub_mod(
				in_bytes[((y * width) + i)*3 + 1],
				ffv1(
					L,
					T,
					TL
				),
				range
			);
			L  = in_bytes[(y * width + i - 1)*3 + 2];
			T  = in_bytes[((y-1) * width + i)*3 + 2];
			TL = in_bytes[((y-1) * width + i - 1)*3 + 2];
			filtered[((y * width) + i)*3 + 2] = sub_mod(
				in_bytes[((y * width) + i)*3 + 2],
				ffv1(
					L,
					T,
					TL
				),
				range
			);
		}
	}

	return filtered;
}

#endif //COLOUR_FILTERS_HEADER
