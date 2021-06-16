#ifndef COLOUR_FILTERS_HEADER
#define COLOUR_FILTERS_HEADER

#include "numerics.hpp"
#include "delta_colour.hpp"
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

uint8_t* colour_filter_all_ffv1_subGreen(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height * 3];

	uint16_t* rsub = new uint16_t[width * height];
	uint16_t* bsub = new uint16_t[width * height];

	for(size_t i=0;i<width*height;i++){
		rsub[i] = range   + in_bytes[i*3 + 1] - delta(255,in_bytes[i*3]);
		bsub[i] = 2*range + in_bytes[i*3 + 2] - delta(255,in_bytes[i*3]);//no need to subtract red
	}

	filtered[0] = in_bytes[0];
	filtered[1] = rsub[0] % range;
	filtered[2] = bsub[0] % range;

	for(size_t i=1;i<width;i++){
		filtered[i*3] = sub_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		filtered[i*3 + 1] = sub_mod(rsub[i],rsub[i-1],range);
		filtered[i*3 + 2] = sub_mod(bsub[i],bsub[i-1],range);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width*3] = sub_mod(in_bytes[y*width*3],in_bytes[(y - 1)*width*3],range);
		filtered[y*width*3 + 1] = sub_mod(rsub[y*width],rsub[(y-1)*width],range);
		filtered[y*width*3 + 2] = sub_mod(bsub[y*width],bsub[(y-1)*width],range);
		for(size_t i=1;i<width;i++){
			uint8_t gL  = in_bytes[(y * width + i - 1)*3];
			uint8_t gT  = in_bytes[((y-1) * width + i)*3];
			uint8_t gTL = in_bytes[((y-1) * width + i - 1)*3];
			filtered[((y * width) + i)*3] = sub_mod(
				in_bytes[((y * width) + i)*3],
				ffv1(
					gL,
					gT,
					gTL
				),
				range
			);
			uint16_t L  = rsub[y * width + i - 1];
			uint16_t T  = rsub[(y-1) * width + i];
			uint16_t TL = rsub[(y-1) * width + i - 1];
			filtered[((y * width) + i)*3 + 1] = sub_mod(
				rsub[(y * width) + i],
				ffv1(
					L,
					T,
					TL
				),
				range
			);
			L  = bsub[y * width + i - 1];
			T  = bsub[(y-1) * width + i];
			TL = bsub[(y-1) * width + i - 1];
			filtered[((y * width) + i)*3 + 2] = sub_mod(
				bsub[(y * width) + i],
				ffv1(
					L,
					T,
					TL
				),
				range
			);
		}
	}
	delete[] rsub;
	delete[] bsub;
	return filtered;
}

#endif //COLOUR_FILTERS_HEADER
