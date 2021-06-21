#ifndef CROSSCOLOUR_OPTIMISER_HEADER
#define CROSSCOLOUR_OPTIMISER_HEADER

#include "delta_colour.hpp"
#include "tile_optimiser.hpp"

void crossColour_map_initial(
	uint8_t* in_bytes,
	uint16_t*& red,
	uint16_t*& blue,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& crossColour_image,
	uint32_t& crossColourWidth,
	uint32_t& crossColourHeight
){
	uint32_t crossColourWidth_block  = 60;
	uint32_t crossColourHeight_block = 60;

	crossColourWidth  = (width + crossColourWidth_block - 1)/crossColourWidth_block;
	crossColourHeight = (height + crossColourHeight_block - 1)/crossColourHeight_block;

	crossColour_image = new uint8_t[crossColourWidth*crossColourHeight];
	red = new uint16_t[width*height];
	blue = new uint16_t[width*height];

	//get rid of these two later:
	uint8_t* filtered_red = new uint8_t[width*height];
	uint8_t* filtered_blue = new uint8_t[width*height];


	for(size_t block=0;block<crossColourWidth*crossColourHeight;block++){
		uint8_t rg = 255;
		tile_delta_transform_red(
			in_bytes,
			red,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			rg
		);
		tile_predict_ffv1(
			red,
			filtered_red,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight
		);
		size_t baseY = (block / crossColourWidth) * crossColourHeight_block;
		size_t baseX = (block % crossColourWidth) * crossColourWidth_block;
		SymbolStats stats;
		size_t count = 0;
		for(size_t i=0;i<256;i++){
			stats.freqs[i] = 0;
		}
		for(size_t y=baseY;y<height && y < (baseY + crossColourHeight_block);y++){
		for(size_t x=baseX;x<width && x < (baseX + crossColourWidth_block);x++){
			size_t location = y*width + x;
			stats.freqs[filtered_red[location]]++;
			count++;
		}
		}
		double best = estimateEntropy_freq(stats, count);

		for(size_t rg_alt=0;rg_alt < 256;rg_alt++){
			tile_delta_transform_red(
				in_bytes,
				red,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight,
				(uint8_t)rg_alt
			);
			tile_predict_ffv1(
				red,
				filtered_red,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			double cost = 0;
			for(size_t y=baseY;y<height && y < (baseY + crossColourHeight_block);y++){
			for(size_t x=baseX;x<width && x < (baseX + crossColourWidth_block);x++){
				size_t location = y*width + x;
				cost += -std::log2((double)stats.freqs[filtered_red[location]]/(double)count);
			}
			}

			if(cost < best){
				best = cost;
				rg = rg_alt;
			}
		}
		tile_delta_transform_red(
			in_bytes,
			red,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			rg
		);
		printf("%d\n",(int)rg);
/*
		uint8_t bg = 0;
		best = 999999999;
		for(size_t bg_alt=0;bg_alt < 256;bg_alt++){
			tile_delta_transform_blue(
				in_bytes,
				blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight,
				(uint8_t)bg_alt,
				0
			);
			tile_predict_ffv1(
				blue,
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			if(cost < best){
				best = cost;
				bg = bg_alt;
			}
		}
		tile_delta_transform_blue(
			in_bytes,
			blue,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			bg,
			0
		);
		uint8_t br = 0;
		best = 999999999;
		for(size_t br_alt=0;br_alt < 256;br_alt++){
			tile_delta_transform_blue(
				in_bytes,
				blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight,
				bg,
				(uint8_t)br_alt
			);
			tile_predict_ffv1(
				blue,
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			if(cost < best){
				best = cost;
				br = br_alt;
			}
		}
		tile_delta_transform_blue(
			in_bytes,
			blue,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			bg,
			br
		);
		best = 999999999;
		for(size_t bg_alt=0;bg_alt < 256;bg_alt++){
			tile_delta_transform_blue(
				in_bytes,
				blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight,
				(uint8_t)bg_alt,
				br
			);
			tile_predict_ffv1(
				blue,
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			if(cost < best){
				best = cost;
				bg = bg_alt;
			}
		}
		tile_delta_transform_blue(
			in_bytes,
			blue,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			bg,
			br
		);
		best = 999999999;
		for(size_t br_alt=0;br_alt < 256;br_alt++){
			tile_delta_transform_blue(
				in_bytes,
				blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight,
				bg,
				(uint8_t)br_alt
			);
			tile_predict_ffv1(
				blue,
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				crossColourWidth,
				crossColourHeight
			);
			if(cost < best){
				best = cost;
				br = br_alt;
			}
		}
		tile_delta_transform_blue(
			in_bytes,
			blue,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			bg,
			br
		);*/
		crossColour_image[block*3] = rg;
		//crossColour_image[block*3+1] = bg;
		//crossColour_image[block*3+2] = br;
		crossColour_image[block*3+1] = 255;
		crossColour_image[block*3+2] = 0;
		tile_delta_transform_blue(
			in_bytes,
			blue,
			range,
			width,
			height,
			block,
			crossColourWidth,
			crossColourHeight,
			255,
			0
		);
	}
	delete[] filtered_red;
	delete[] filtered_blue;
}

double crossColour_badness_red(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	size_t tileIndex,
	uint32_t& crossColourWidth,
	uint32_t& crossColourHeight,
	uint8_t rg
){
	double badness = 0;
	size_t count = 0;
	uint32_t crossColourWidth_block  = (width + crossColourWidth - 1)/crossColourWidth;
	uint32_t crossColourHeight_block = (height + crossColourHeight - 1)/crossColourHeight;
	size_t baseY = (tileIndex / crossColourWidth) * crossColourHeight_block;
	size_t baseX = (tileIndex % crossColourWidth) * crossColourWidth_block;
	uint16_t cache = 0;
	SymbolStats stats;
	for(size_t i=0;i<256;i++){
		stats.freqs[i] = 0;
	}
	for(size_t y=baseY;y<height && y < (baseY + crossColourHeight_block);y++){
	for(size_t x=baseX+1;x<width && x < (baseX + crossColourWidth_block);x++){
		size_t location = y*width + x;
		uint16_t red = range + in_bytes[location*3+1] - delta(rg,in_bytes[location*3+0]);
		stats.freqs[(2*range + red - cache) % range]++	;
		cache = red;
		count++;
	}
	}
	return estimateEntropy_freq(stats, count);
}

#endif //CROSSCOLOUR_OPTIMISER
