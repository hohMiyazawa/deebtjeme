#ifndef COLOUR_FILTERS_HEADER
#define COLOUR_FILTERS_HEADER

#include "numerics.hpp"

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
			filtered[(y*width + i)*3 + 1] = (uint8_t)(
				(
					
					(
						((int)in_bytes[(y*width + i)*3 + 1] - (int)in_bytes[(y*width + i)*3])
						- ((int)in_bytes[(y*width + i - 1)*3 + 1] - (int)in_bytes[(y*width + i - 1)*3])
					)
					+ 2*range
				) % range
			);
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

#endif //COLOUR_FILTERS_HEADER
