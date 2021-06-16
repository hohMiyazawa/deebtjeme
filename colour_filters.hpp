#ifndef COLOUR_FILTERS_HEADER
#define COLOUR_FILTERS_HEADER

#include "numerics.hpp"
#include "delta_colour.hpp"
#include "filter_utils.hpp"
#include "colour_filter_utils.hpp"

uint8_t* colour_filter_all_left(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
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
			filtered[((y * width) + i)*3] = sub_mod(
				in_bytes[((y * width) + i)*3],
				L,
				range
			);
			L  = in_bytes[(y * width + i - 1)*3 + 1];
			filtered[((y * width) + i)*3 + 1] = sub_mod(
				in_bytes[((y * width) + i)*3 + 1],
				L,
				range
			);
			L  = in_bytes[(y * width + i - 1)*3 + 2];
			filtered[((y * width) + i)*3 + 2] = sub_mod(
				in_bytes[((y * width) + i)*3 + 2],
				L,
				range
			);
		}
	}

	return filtered;
}

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

uint8_t* hbd_filter_all_ffv1(uint16_t* in_bytes, uint32_t range, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];

	for(size_t i=1;i<width;i++){
		filtered[i] = sub_mod(in_bytes[i],in_bytes[i - 1],range);
	}

	for(size_t y=1;y<height;y++){
		filtered[y*width] = sub_mod(in_bytes[y*width],in_bytes[(y - 1)*width],range);
		for(size_t i=1;i<width;i++){
			uint16_t L  = in_bytes[(y * width + i - 1)];
			uint16_t T  = in_bytes[((y-1) * width + i)];
			uint16_t TL = in_bytes[((y-1) * width + i - 1)];
			filtered[(y * width) + i] = sub_mod(
				in_bytes[(y * width) + i],
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

uint8_t* colour_filter_all_subGreen(uint8_t* in_bytes, uint32_t range, uint32_t width, uint32_t height, uint16_t predictor){
	if(predictor == 0){
		return colour_filter_all_ffv1_subGreen(in_bytes, range, width, height);
	}
	int a = (predictor & 0b1111000000000000) >> 12;
	int b = (predictor & 0b0000111100000000) >> 8;
	int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
	int d = (predictor & 0b0000000000001111);
	int sum = a + b + c + d;
	int halfsum = sum >> 1;

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
			uint8_t gTR = in_bytes[((y-1) * width + i + 1)*3];
			filtered[((y * width) + i)*3] = sub_mod(
				in_bytes[((y * width) + i)*3],
				clamp(
					(
						a*gL + b*gT + c*gTL + d*gTR + halfsum
					)/sum,
					range
				),
				range
			);
			uint16_t L  = rsub[y * width + i - 1];
			uint16_t T  = rsub[(y-1) * width + i];
			uint16_t TL = rsub[(y-1) * width + i - 1];
			uint16_t TR = rsub[(y-1) * width + i + 1];
			filtered[((y * width) + i)*3 + 1] = sub_mod(
				rsub[(y * width) + i],
				i_clamp(
					(
						a*L + b*T + c*TL + d*TR + halfsum
					)/sum,
					0,
					2*range
				),
				range
			);
			L  = bsub[y * width + i - 1];
			T  = bsub[(y-1) * width + i];
			TL = bsub[(y-1) * width + i - 1];
			TR = bsub[(y-1) * width + i + 1];
			filtered[((y * width) + i)*3 + 2] = sub_mod(
				bsub[(y * width) + i],
				i_clamp(
					(
						a*L + b*T + c*TL + d*TR + halfsum
					)/sum,
					0,
					3*range
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
