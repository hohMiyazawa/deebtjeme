#ifndef TILE_OPTIMISER_HEADER
#define TILE_OPTIMISER_HEADER

#include "delta_colour.hpp"
#include "colour_filter_utils.hpp"
#include "entropy_estimation.hpp"
#include "symbolstats.hpp"

void tile_delta_transform_red(
	uint8_t* in_bytes,
	uint16_t* red,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	size_t tileIndex,
	uint32_t tile_width,
	uint32_t tile_height,
	uint8_t rg
){
	uint32_t tile_width_block  = (width + tile_width - 1)/tile_width;
	uint32_t tile_height_block  = (height + tile_height - 1)/tile_height;
	size_t baseY = (tileIndex/tile_width) * tile_height_block;
	size_t baseX = (tileIndex%tile_width) * tile_width_block;
	for(size_t y=baseY;y<height && y < (baseY + tile_height_block);y++){
	for(size_t x=baseX;x<width && x < (baseX + tile_width_block);x++){
		size_t location = y*width + x;
		red[location] = range + in_bytes[location*3 + 1] - delta(rg,in_bytes[location*3 + 0]);
		//printf("%d\n",(int)red[location]);
	}
	}
}

void tile_delta_transform_blue(
	uint8_t* in_bytes,
	uint16_t* blue,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	size_t tileIndex,
	uint32_t tile_width,
	uint32_t tile_height,
	uint8_t bg,
	uint8_t br
){
	uint32_t tile_width_block  = (width + tile_width - 1)/tile_width;
	uint32_t tile_height_block  = (height + tile_height - 1)/tile_height;
	size_t baseY = (tileIndex/tile_width) * tile_height_block;
	size_t baseX = (tileIndex%tile_width) * tile_width_block;
	for(size_t y=baseY;y<height && y < (baseY + tile_height_block);y++){
	for(size_t x=baseX;x<width && x < (baseX + tile_width_block);x++){
		size_t location = y*width + x;
		blue[location] = 2*range + in_bytes[location*3 + 2] - delta(bg,in_bytes[location*3 + 0]) - delta(br,in_bytes[location*3 + 1]);
	}
	}
}

void tile_predict_ffv1(
	uint16_t* colour_channel,
	uint8_t* residual,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	size_t tileIndex,
	uint32_t tile_width,
	uint32_t tile_height
){
	uint32_t tile_width_block  = (width + tile_width - 1)/tile_width;
	uint32_t tile_height_block  = (height + tile_height - 1)/tile_height;
	size_t baseY = (tileIndex/tile_width) * tile_height_block;
	size_t baseX = (tileIndex%tile_width) * tile_width_block;
	for(size_t y=baseY;y<height && y < (baseY + tile_height_block);y++){
	for(size_t x=baseX;x<width && x < (baseX + tile_width_block);x++){
		size_t location = y*width + x;
		if(y == 0){
			if(x == 0){
				residual[location] = colour_channel[0] % range;
			}
			else{
				residual[location] = (
					3*range
					+ colour_channel[location]
					- colour_channel[location - 1]
				) % range;
			}
		}
		else{
			if(x == 0){
				residual[location] = (
					3*range
					+ colour_channel[location]
					- colour_channel[location - width]
				) % range;
			}
			else{
				residual[location] = (
					3*range
					+ colour_channel[location]
					- ffv1(
						colour_channel[location - 1],
						colour_channel[location - width],
						colour_channel[location - width - 1]
					)
				) % range;
			}
		}
	}
	}
}

double tile_residual_entropy(
	uint8_t* residual,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	size_t tileIndex,
	uint32_t tile_width,
	uint32_t tile_height
){
	uint32_t tile_width_block  = (width + tile_width - 1)/tile_width;
	uint32_t tile_height_block  = (height + tile_height - 1)/tile_height;
	size_t baseY = (tileIndex/tile_width) * tile_height_block;
	size_t baseX = (tileIndex%tile_width) * tile_width_block;
	SymbolStats stats;
	size_t count = 0;
	for(size_t i=0;i<256;i++){
		stats.freqs[i] = 0;
	}
	for(size_t y=baseY;y<height && y < (baseY + tile_height_block);y++){
	for(size_t x=baseX;x<width && x < (baseX + tile_width_block);x++){
		size_t location = y*width + x;
		stats.freqs[residual[location]]++;
		count++;
	}
	}
	return estimateEntropy_freq(stats, count);
}

void colourChannel_predict_ffv1(
	uint16_t* colour_channel,
	uint8_t* residual,
	uint32_t range,
	uint32_t width,
	uint32_t height
){
	for(size_t location=0;location < width*height;location++){
		if(location == 0){
			residual[location] = colour_channel[0] % range;
		}
		else if(location < width){
			residual[location] = (
				3*range
				+ colour_channel[location]
				- colour_channel[location - 1]
			) % range;
		}
		else if(location % width == 0){
			residual[location] = (
				3*range
				+ colour_channel[location]
				- colour_channel[location - width]
			) % range;
		}
		else{
			residual[location] = (
				3*range
				+ colour_channel[location]
				- ffv1(
					colour_channel[location - 1],
					colour_channel[location - width],
					colour_channel[location - width - 1]
				)
			) % range;
		}
	}
}

#endif //TILE_OPTIMISER
