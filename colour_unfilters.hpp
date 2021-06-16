#ifndef COLOUR_UNFILTERS_HEADER
#define COLOUR_UNFILTERS_HEADER

#include "numerics.hpp"
#include "2dutils.hpp"
#include "filter_utils.hpp"
#include "delta_colour.hpp"
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

void colourSub_unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* colourImage,
	uint32_t colourWidth,
	uint32_t colourHeight,
	uint16_t predictor
){
	uint32_t colourWidth_block  = (width  + colourWidth  - 1)/colourWidth;
	uint32_t colourHeight_block = (height + colourHeight - 1)/colourHeight;

	uint16_t rcache[width];
	uint16_t bcache[width];
	uint16_t rcache_L = 0;
	uint16_t bcache_L = 0;
	if(predictor == 0){
		for(size_t i=0;i<width*height;i++){
			size_t colourIndex = tileIndexFromPixel(
				i,
				width,
				colourWidth,
				colourWidth_block,
				colourHeight_block
			);
	//green first
			if(i == 0){
			}
			else if(i < width){
				in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
			}
			else if(i % width == 0){
				in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - width) * 3],range);
			}
			else{
				uint8_t L  = in_bytes[(i - 1)*3];
				uint8_t TL = in_bytes[(i - width - 1)*3];
				uint8_t T  = in_bytes[(i - width)*3];
				uint8_t here = in_bytes[i*3];
				in_bytes[i*3] = add_mod(
					here,
					ffv1(
						L,
						T,
						TL
					),
					range
				);
			}
			//printf("i %d\n",(int)i);
			//printf("inded %d\n",(int)colourIndex);
	//combined red-blue decode
			uint16_t rg_delta = delta(colourImage[colourIndex*3],in_bytes[i*3]);
			if(i == 0){
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rg_delta) % range;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else if(i < width){
				//printf("test %d %d %d %d\n",(int)in_bytes[i*3 + 1],(int)rg_delta, (int)rcache_L,(int)in_bytes[i*3]);
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache_L + rg_delta) % range;
				//printf("test %d\n",(int)in_bytes[i*3 + 1]);
				rcache[i - 1] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else if(i % width == 0){
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache[0] + rg_delta) % range;
				rcache[(i - 1) % width] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else{
				uint16_t L = rcache_L;
				uint16_t T = rcache[i % width];
				uint16_t TL = rcache[(i-1) % width];
				in_bytes[i*3 + 1] = (
					in_bytes[i*3 + 1]
					+ ffv1(L,T,TL)
					+ rg_delta
				) % range;
				rcache[(i - 1) % width] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			uint16_t bg_delta = delta(colourImage[colourIndex*3 + 1],in_bytes[i*3]);
			uint16_t br_delta = delta(colourImage[colourIndex*3 + 2],in_bytes[i*3 + 1]);
			if(i == 0){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bg_delta + br_delta) % range;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else if(i < width){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache_L + bg_delta + br_delta) % range;
				bcache[i - 1] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else if(i % width == 0){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache[0] + bg_delta + br_delta) % range;
				bcache[(i - 1) % width] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else{
				uint16_t L = bcache_L;
				uint16_t T = bcache[i % width];
				uint16_t TL = bcache[(i-1) % width];
				in_bytes[i*3 + 2] = (
					in_bytes[i*3 + 2]
					+ ffv1(L,T,TL)
					+ bg_delta
					+ br_delta
				) % range;
				bcache[(i - 1) % width] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
		}
	}
	else{
		int a = (predictor & 0b1111000000000000) >> 12;
		int b = (predictor & 0b0000111100000000) >> 8;
		int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
		int d = (predictor & 0b0000000000001111);
		uint8_t sum = a + b + c + d;
		uint8_t halfsum = sum >> 1;
		for(size_t i=0;i<width*height;i++){
			size_t colourIndex = tileIndexFromPixel(
				i,
				width,
				colourWidth,
				colourWidth_block,
				colourHeight_block
			);
	//green first
			if(i == 0){
			}
			else if(i < width){
				in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
			}
			else if(i % width == 0){
				in_bytes[i * 3] = add_mod(in_bytes[i * 3],in_bytes[(i - width) * 3],range);
			}
			else{
				uint8_t L = in_bytes[(i - 1)*3];
				uint8_t TL = in_bytes[(i - width - 1)*3];
				uint8_t T = in_bytes[(i - width)*3];
				uint8_t TR = in_bytes[(i - width + 1)*3];
				uint8_t here = in_bytes[i*3];
				in_bytes[i*3] = add_mod(
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
	//combined red-blue decode
			uint16_t rg_delta = delta(colourImage[i*3],in_bytes[i*3]);
			if(i == 0){
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rg_delta) % range;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else if(i < width){
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache_L + rg_delta) % range;
				rcache[i - 1] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else if(i % width == 0){
				in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache[0] + rg_delta) % range;
				rcache[(i - 1) % width] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			else{
				uint16_t L = rcache_L;
				uint16_t T = rcache[i % width];
				uint16_t TL = rcache[(i-1) % width];
				uint16_t TR = rcache[(i+1) % width];
				in_bytes[i*3 + 1] = (
					in_bytes[i*3 + 1]
					+ i_clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						0,
						2*range
					)
					+ rg_delta
				) % range;
				rcache[(i - 1) % width] = rcache_L;
				rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
			}
			uint16_t bg_delta = delta(colourImage[i*3 + 1],in_bytes[i*3]);
			uint16_t br_delta = delta(colourImage[i*3 + 2],in_bytes[i*3 + 1]);
			if(i == 0){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bg_delta + br_delta) % range;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else if(i < width){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache_L + bg_delta + br_delta) % range;
				bcache[i - 1] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else if(i % width == 0){
				in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache[0] + bg_delta + br_delta) % range;
				bcache[(i - 1) % width] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
			else{
				uint16_t L = bcache_L;
				uint16_t T = bcache[i % width];
				uint16_t TL = bcache[(i-1) % width];
				uint16_t TR = bcache[(i+1) % width];
				in_bytes[i*3 + 2] = (
					in_bytes[i*3 + 2]
					+ i_clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						0,
						3*range
					)
					+ bg_delta
					+ br_delta
				) % range;
				bcache[(i - 1) % width] = bcache_L;
				bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
			}
		}
	}
}

void colourSub_unfilter_all(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* colourImage,
	uint32_t colourWidth,
	uint32_t colourHeight,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t colourWidth_block  = (width  + colourWidth  - 1)/colourWidth;
	uint32_t colourHeight_block = (height + colourHeight - 1)/colourHeight;
	uint32_t predictorWidth_block  = (width  + predictorWidth  - 1)/predictorWidth;
	uint32_t predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;

	uint16_t rcache[width];
	uint16_t bcache[width];
	uint16_t rcache_L = 0;
	uint16_t bcache_L = 0;

	for(size_t i=0;i<width*height;i++){
		size_t colourIndex = tileIndexFromPixel(
			i,
			width,
			colourWidth,
			colourWidth_block,
			colourHeight_block
		);
		size_t predictorIndex = tileIndexFromPixel(
			i,
			width,
			predictorWidth,
			predictorWidth_block,
			predictorHeight_block
		);
//green first
		if(i == 0){
		}
		else if(i < width){
			in_bytes[i*3] = add_mod(in_bytes[i*3],in_bytes[(i - 1)*3],range);
		}
		else if(i % width == 0){
			in_bytes[i * 3] = add_mod(in_bytes[i * 3],in_bytes[(i - width) * 3],range);
		}
		else{
			uint16_t predictor = predictorImage[predictorIndex*3];
			uint8_t L = in_bytes[(i - 1)*3];
			uint8_t TL = in_bytes[(i - width - 1)*3];
			uint8_t T = in_bytes[(i - width)*3];
			uint8_t here = in_bytes[i*3];

			if(predictor == 0){
				in_bytes[i*3] = add_mod(
					here,
					ffv1(
						L,
						T,
						TL
					),
					range
				);
			}
			else{
				int a = (predictor & 0b1111000000000000) >> 12;
				int b = (predictor & 0b0000111100000000) >> 8;
				int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
				int d = (predictor & 0b0000000000001111);
				uint8_t TR = in_bytes[(i - width + 1)*3];
				uint8_t sum = a + b + c + d;
				uint8_t halfsum = sum >> 1;
				in_bytes[i*3] = add_mod(
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
//combined red-blue decode
		uint16_t rg_delta = delta(colourImage[colourIndex*3],in_bytes[i*3]);
		if(i == 0){
			in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rg_delta) % range;
			rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
		}
		else if(i < width){
			in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache_L + rg_delta) % range;
			rcache[i - 1] = rcache_L;
			rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
		}
		else if(i % width == 0){
			in_bytes[i*3 + 1] = (in_bytes[i*3 + 1] + rcache[0] + rg_delta) % range;
			rcache[(i - 1) % width] = rcache_L;
			rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
		}
		else{
			uint16_t predictor = predictorImage[predictorIndex*3 + 1];
			int a = (predictor & 0b1111000000000000) >> 12;
			int b = (predictor & 0b0000111100000000) >> 8;
			int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
			int d = (predictor & 0b0000000000001111);
			uint8_t sum = a + b + c + d;
			uint8_t halfsum = sum >> 1;
			uint16_t L = rcache_L;
			uint16_t T = rcache[i % width];
			uint16_t TL = rcache[(i-1) % width];
			uint16_t TR = rcache[(i+1) % width];
			if(predictor == 0){
				in_bytes[i*3 + 1] = (
					in_bytes[i*3 + 1]
					+ ffv1(L,T,TL)
					+ rg_delta
				) % range;
			}
			else{
				in_bytes[i*3 + 1] = (
					in_bytes[i*3 + 1]
					+ i_clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						0,
						2*range
					)
					+ rg_delta
				) % range;
			}
			rcache[(i - 1) % width] = rcache_L;
			rcache_L = in_bytes[i*3 + 1] + range - rg_delta;
		}
		uint16_t bg_delta = delta(colourImage[colourIndex*3 + 1],in_bytes[i*3]);
		uint16_t br_delta = delta(colourImage[colourIndex*3 + 2],in_bytes[i*3 + 1]);
		if(i == 0){
			in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bg_delta + br_delta) % range;
			bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
		}
		else if(i < width){
			in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache_L + bg_delta + br_delta) % range;
			bcache[i - 1] = bcache_L;
			bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
		}
		else if(i % width == 0){
			in_bytes[i*3 + 2] = (in_bytes[i*3 + 2] + bcache[0] + bg_delta + br_delta) % range;
			bcache[(i - 1) % width] = bcache_L;
			bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
		}
		else{
			uint16_t predictor = predictorImage[predictorIndex*3 + 2];
			int a = (predictor & 0b1111000000000000) >> 12;
			int b = (predictor & 0b0000111100000000) >> 8;
			int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
			int d = (predictor & 0b0000000000001111);
			int sum = a + b + c + d;
			int halfsum = sum >> 1;
			uint16_t L = bcache_L;
			uint16_t T = bcache[i % width];
			uint16_t TL = bcache[(i-1) % width];
			uint16_t TR = bcache[(i+1) % width];
			if(predictor == 0){
				in_bytes[i*3 + 2] = (
					in_bytes[i*3 + 2]
					+ ffv1(L,T,TL)
					+ bg_delta
					+ br_delta
				) % range;
			}
			else{
				in_bytes[i*3 + 2] = (
					in_bytes[i*3 + 2]
					+ i_clamp(
						(
							a*L + b*T + c*TL + d*TR + halfsum
						)/sum,
						0,
						3*range
					)
					+ bg_delta
					+ br_delta
				) % range;
			}
			bcache[(i - 1) % width] = bcache_L;
			bcache_L = in_bytes[i*3 + 2] + 2*range - bg_delta - br_delta;
		}
	}
}

#endif //COLOUR_UNFILTERS_HEADER
