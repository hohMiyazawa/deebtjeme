#ifndef COLOURMAP_ENCODER_HEADER
#define COLOURMAP_ENCODER_HEADER

#include "tile_optimiser.hpp"
#include "filters.hpp"
#include "varint.hpp"
#include "colour_simple_encoders.hpp"

void colourMap_encoder(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t*  green = new uint8_t[width*height];
	uint16_t* red   = new uint16_t[width*height];
	uint16_t* blue  = new uint16_t[width*height];
	uint8_t* filtered_red = new uint8_t[width*height];
	uint8_t* filtered_blue = new uint8_t[width*height];

	for(size_t i=0;i<width*height;i++){
		green[i] = in_bytes[i*3];
	}

/*
	tile_delta_transform_red(
		in_bytes,
		red,
		range,
		width,
		height,
		0,
		1,
		1,
		255
	);
*/
	size_t blockSizex = 64;
	size_t blockSizey = 64;
	size_t tile_width = (width + blockSizex - 1)/blockSizex;
	size_t tile_height = (height + blockSizey - 1)/blockSizey;

	uint8_t* colour_image = new uint8_t[tile_width*tile_height*3];

	for(size_t block=0;block < tile_width*tile_height;block++){
		uint8_t rg = 0;
		double best = 999999999;
		for(size_t rg_alt=0;rg_alt < 256;rg_alt++){
			tile_delta_transform_red(
				in_bytes,
				red,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height,
				(uint8_t)rg_alt
			);
			tile_predict_ffv1(
				red,
				filtered_red,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
			);
			double cost = tile_residual_entropy(
				filtered_red,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
			);
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
			tile_width,
			tile_height,
			rg
		);
		colour_image[block*3] = rg;
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
				tile_width,
				tile_height,
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
				tile_width,
				tile_height
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
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
			tile_width,
			tile_height,
			bg,
			0
		);
		colour_image[block*3+1] = bg;
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
				tile_width,
				tile_height,
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
				tile_width,
				tile_height
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
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
			tile_width,
			tile_height,
			bg,
			br
		);
		colour_image[block*3+2] = br;
		best = 999999999;
		for(size_t bg_alt=0;bg_alt < 256;bg_alt++){
			tile_delta_transform_blue(
				in_bytes,
				blue,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height,
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
				tile_width,
				tile_height
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
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
			tile_width,
			tile_height,
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
				tile_width,
				tile_height,
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
				tile_width,
				tile_height
			);
			double cost = tile_residual_entropy(
				filtered_blue,
				range,
				width,
				height,
				block,
				tile_width,
				tile_height
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
			tile_width,
			tile_height,
			bg,
			br
		);
	}
/*
	tile_delta_transform_blue(
		in_bytes,
		blue,
		range,
		width,
		height,
		0,
		1,
		1,
		255,
		0
	);
*/

	uint8_t* filtered_green = filter_all_ffv1(green, range, width, height);

	colourChannel_predict_ffv1(
		red,
		filtered_red,
		range,
		width,
		height
	);
	colourChannel_predict_ffv1(
		blue,
		filtered_blue,
		range,
		width,
		height
	);

	SymbolStats stats_green;
	stats_green.count_freqs(filtered_green, width*height);
	SymbolStats stats_red;
	stats_red.count_freqs(filtered_red, width*height);
	SymbolStats stats_blue;
	stats_blue.count_freqs(filtered_blue, width*height);
	/*for(size_t i=0;i<256;i++){
		printf("%d\n",(int)stats_red.freqs[i]);
	}*/

	BitWriter tableEncode;
	SymbolStats table_green = encode_freqTable(stats_green,tableEncode,range);
	SymbolStats table_red = encode_freqTable(stats_red,tableEncode,range);
	SymbolStats table_blue = encode_freqTable(stats_blue,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms_green[256];
	RansEncSymbol esyms_red[256];
	RansEncSymbol esyms_blue[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms_green[i], table_green.cum_freqs[i], table_green.freqs[i], 16);
		RansEncSymbolInit(&esyms_red[i],   table_red.cum_freqs[i],   table_red.freqs[i], 16);
		RansEncSymbolInit(&esyms_blue[i],  table_blue.cum_freqs[i],  table_blue.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms_blue  + filtered_blue[index]);
		RansEncPutSymbol(&rans, &outPointer, esyms_red   + filtered_red[index]);
		RansEncPutSymbol(&rans, &outPointer, esyms_green + filtered_green[index]);
	}
	RansEncFlush(&rans, &outPointer);

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	uint8_t* entropy_image = new uint8_t[3];
	entropy_image[0] = 0;
	entropy_image[1] = 1;
	entropy_image[2] = 2;
	colour_encode_raw(
		entropy_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 3 - 1;

	*(--outPointer) = 0b00000000;//one predictor: ffv1
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 1 - 1;

/*
	uint8_t* colour_image = new uint8_t[3];
	colour_image[0] = 255;
	colour_image[1] = 255;
	colour_image[2] = 0;
	colour_encode_raw(
		colour_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] colour_image;
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);
*/
	uint8_t* trailing = outPointer;
	colour_encode_ffv1(
		colour_image,
		256,
		tile_width,
		tile_height,
		outPointer
	);
	delete[] colour_image;
	printf("---colour image size: %d bytes\n",(int)(trailing - outPointer));
	writeVarint_reverse((uint32_t)(tile_height - 1),outPointer);
	writeVarint_reverse((uint32_t)(tile_width - 1), outPointer);

	*(--outPointer) = 0b00110110;

	delete[] green;
	delete[] filtered_green;
	delete[] red;
	delete[] filtered_red;
	delete[] blue;
	delete[] filtered_blue;
}

#endif //COLOURMAP_ENCODER
