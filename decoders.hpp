#ifndef DECODERS_HEADER
#define DECODERS_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "bitreader.hpp"
#include "unfilters.hpp"
#include "varint.hpp"
#include "colour_unfilters.hpp"

uint8_t* decode_colourMap_entropyMap_predictionMap_lz_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* colourImage,
	uint32_t colourWidth,
	uint32_t colourHeight,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint32_t predictorWidth_block  = (width + predictorWidth - 1)/predictorWidth;
	uint32_t predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;

	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);
		if(i == 0){
			image[i*3 + 0] = s;
		}
		else if(i < width){
			image[i*3 + 0] = add_mod(s,image[(i-1)*3 + 0],range);
		}
		else if(i % width == 0){
			image[i*3 + 0] = add_mod(s,image[(i-width)*3],range);
		}
		else{
			size_t predictorIndex = tileIndexFromPixel(
				i,
				width,
				predictorWidth,
				predictorWidth_block,
				predictorHeight_block
			);
			uint16_t predictor = predictorImage[predictorIndex*3];
			int a = (predictor & 0b1111000000000000) >> 12;
			int b = (predictor & 0b0000111100000000) >> 8;
			int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
			int d = (predictor & 0b0000000000001111);

			uint8_t sum = a + b + c + d;
			uint8_t halfsum = sum >> 1;
			if(predictor == 0){
				image[i*3] = add_mod(
					s,
					ffv1(
						image[(i-1)*3],
						image[(i-width)*3],
						image[(i-width-1)*3]
					),
					range
				);
			}
			else{
				image[i*3] = add_mod(
					s,
					clamp(
						(
							a*image[(i-1)*3] + b*image[(i-width)*3] + c*image[(i-width-1)*3] + d*image[(i-width+1)*3] + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}

	return image;
}

uint8_t* decode_entropyMap_lz_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;

	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);
		image[i*3 + 0] = s;


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}

	return image;
}

uint8_t* decode_entropyMap_predictionMap_lz(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint32_t  predictorWidth_block  = (width + predictorWidth - 1)/predictorWidth;
	uint32_t  predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}
		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);
		if(i == 0){
			image[i] = s;
		}
		else if(i < width){
			image[i] = add_mod(s,image[i-1],range);
		}
		else if(i % width == 0){
			image[i] = add_mod(s,image[i-width],range);
		}
		else{
			size_t predictorIndex = tileIndexFromPixel(
				i,
				width,
				predictorWidth,
				predictorWidth_block,
				predictorHeight_block
			);
			uint16_t predictor = predictorImage[predictorIndex];
			int a = (predictor & 0b1111000000000000) >> 12;
			int b = (predictor & 0b0000111100000000) >> 8;
			int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
			int d = (predictor & 0b0000000000001111);

			uint8_t sum = a + b + c + d;
			uint8_t halfsum = sum >> 1;
			if(predictor == 0){
				image[i] = add_mod(
					s,
					ffv1(
						image[i-1],
						image[i-width],
						image[i-width-1]
					),
					range
				);
			}
			else{
				image[i] = add_mod(
					s,
					clamp(
						(
							a*image[i-1] + b*image[i-width] + c*image[i-width-1] + d*image[i-width+1] + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}
	}
	return image;
}

uint8_t* decode_entropy_predictionMap_lz(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats table,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t  predictorWidth_block  = (width + predictorWidth - 1)/predictorWidth;
	uint32_t  predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}

		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
		if(i == 0){
			image[i] = s;
		}
		else if(i < width){
			image[i] = add_mod(s,image[i-1],range);
		}
		else if(i % width == 0){
			image[i] = add_mod(s,image[i-width],range);
		}
		else{
			size_t predictorIndex = tileIndexFromPixel(
				i,
				width,
				predictorWidth,
				predictorWidth_block,
				predictorHeight_block
			);
			uint16_t predictor = predictorImage[predictorIndex];
			int a = (predictor & 0b1111000000000000) >> 12;
			int b = (predictor & 0b0000111100000000) >> 8;
			int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
			int d = (predictor & 0b0000000000001111);

			uint8_t sum = a + b + c + d;
			uint8_t halfsum = sum >> 1;
			if(predictor == 0){
				image[i] = add_mod(
					s,
					ffv1(
						image[i-1],
						image[i-width],
						image[i-width-1]
					),
					range
				);
			}
			else{
				image[i] = add_mod(
					s,
					clamp(
						(
							a*image[i-1] + b*image[i-width] + c*image[i-width-1] + d*image[i-width+1] + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}
	}
	return image;
}

uint8_t* decode_entropyMap_prediction_lz(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t predictor
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;

	int a = (predictor & 0b1111000000000000) >> 12;
	int b = (predictor & 0b0000111100000000) >> 8;
	int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
	int d = (predictor & 0b0000000000001111);

	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}
		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);
		if(i == 0){
			image[i] = s;
		}
		else if(i < width){
			image[i] = add_mod(s,image[i-1],range);
		}
		else if(i % width == 0){
			image[i] = add_mod(s,image[i-width],range);
		}
		else{
			if(predictor == 0){
				image[i] = add_mod(
					s,
					ffv1(
						image[i-1],
						image[i-width],
						image[i-width-1]
					),
					range
				);
			}
			else{
				image[i] = add_mod(
					s,
					clamp(
						(
							a*image[i-1] + b*image[i-width] + c*image[i-width-1] + d*image[i-width+1] + halfsum
						)/sum,
						range
					),
					range
				);
			}
		}
	}
	return image;
}

uint8_t* decode_raw(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t* image = new uint8_t[width*height];
	for(size_t i=0;i<width*height;i++){
		image[i] = *(fileIndex++);
	}
	return image;
}

uint8_t* decode_lz_raw(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t* image = new uint8_t[width*height];

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		image[i] = *(fileIndex++);
	}
	return image;
}

uint8_t* decode_raw_colour(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t* image = new uint8_t[width*height*3];
	for(size_t i=0;i<width*height;i++){
		image[i*3 + 0] = *(fileIndex++);
		image[i*3 + 1] = *(fileIndex++);
		image[i*3 + 2] = *(fileIndex++);
	}
	return image;
}

uint8_t* decode_lz_raw_colour(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t* image = new uint8_t[width*height*3];

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		image[i*3 + 0] = *(fileIndex++);
		image[i*3 + 1] = *(fileIndex++);
		image[i*3 + 2] = *(fileIndex++);
	}
	return image;
}

uint8_t* decode_entropy_colour(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height, SymbolStats table){
	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height*3;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	return image;
}

uint8_t* decode_entropy_lz_colour(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height, SymbolStats table){
	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height*3;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[(i + t)*3 + 0] = image[(i + t - backref)*3 + 0];
				image[(i + t)*3 + 1] = image[(i + t - backref)*3 + 1];
				image[(i + t)*3 + 2] = image[(i + t - backref)*3 + 2];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	return image;
}

uint8_t* decode_entropy(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height, SymbolStats table){
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	return image;
}

uint8_t* decode_entropy_lz(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height, SymbolStats table){
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}
		else{
			lz_next--;
		}
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	return image;
}

uint8_t* decode_entropyMap_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight
	
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 0] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}
	return image;
}

uint8_t* decode_entropy_prediction_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats table,
	uint16_t predictor
){
	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height*3;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	colour_unfilter_all(
		image,
		range,
		width,
		height,
		predictor
	);
	return image;
}

uint8_t* decode_entropy_prediction(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats table,
	uint16_t predictor
){
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	unfilter_all(
		image,
		range,
		width,
		height,
		predictor
	);
	return image;
}

uint8_t* decode_entropy_prediction_lz(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats table,
	uint16_t predictor
){
	uint8_t* image = new uint8_t[width*height];

	int a = (predictor & 0b1111000000000000) >> 12;
	int b = (predictor & 0b0000111100000000) >> 8;
	int c = (int)((predictor & 0b0000000011110000) >> 4) - 13;
	int d = (predictor & 0b0000000000001111);

	uint8_t sum = a + b + c + d;
	uint8_t halfsum = sum >> 1;

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	uint32_t lz_next = readVarint(fileIndex);
	for(size_t i=0;i<width*height;i++){
		if(lz_next == 0){
			uint32_t backref = readVarint(fileIndex) + 1;
			uint32_t matchlen = readVarint(fileIndex) + 1;
			lz_next = readVarint(fileIndex);
			for(size_t t=0;t<matchlen;t++){
				image[i + t] = image[i + t - backref];
			}
			i += (matchlen - 1);
			continue;
		}
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;
		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		if(i == 0){
			image[i] = s;
		}
		else if(i < width){
			image[i] = add_mod(s,image[i-1],range);
		}
		else if(i % width == 0){
			image[i] = add_mod(s,image[i-width],range);
		}
		else{
			image[i] = add_mod(
				s,
				clamp(
					(
						a*image[i-1] + b*image[i-width] + c*image[i-width-1] + d*image[i-width+1] + halfsum
					)/sum,
					range
				),
				range
			);
		}
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
	}
	return image;
}

uint8_t* decode_entropyMap_prediction_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t predictor
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 0] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}
	colour_unfilter_all(
		image,
		range,
		width,
		height,
		predictor
	);
	return image;
}

uint8_t* decode_entropyMap_prediction(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t predictor
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);

	}
	unfilter_all(
		image,
		range,
		width,
		height,
		predictor
	);
	return image;
}

uint8_t* decode_entropy_predictionMap(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats table,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[256];
	for(size_t i=0;i<256;i++){
		RansDecSymbolInit(&dsyms[i], table.cum_freqs[i], table.freqs[i]);
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		for(size_t j=0;j<256;j++){
			if(table.cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);

	}
	unfilter_all(
		image,
		range,
		width,
		height,
		predictorImage,
		predictorWidth,
		predictorHeight
	);
	return image;
}

uint8_t* decode_entropyMap_predictionMap(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t  entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t  entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
	uint8_t* image = new uint8_t[width*height];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);

	}
	unfilter_all(
		image,
		range,
		width,
		height,
		predictorImage,
		predictorWidth,
		predictorHeight
	);
	return image;
}

uint8_t* decode_colourMap_entropyMap_prediction_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* colourImage,
	uint32_t colourWidth,
	uint32_t colourHeight,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t predictor
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;

	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 0] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}
	printf("residuals decoded\n");
	colourSub_unfilter_all(
		image,
		range,
		width,
		height,
		colourImage,
		colourWidth,
		colourHeight,
		predictor
	);

	return image;
}

uint8_t* decode_colourMap_entropyMap_predictionMap_colour(
	uint8_t*& fileIndex,
	size_t range,
	uint32_t width,
	uint32_t height,
	uint8_t* colourImage,
	uint32_t colourWidth,
	uint32_t colourHeight,
	SymbolStats* tables,
	uint8_t* entropyImage,
	uint8_t entropyContexts,
	uint32_t entropyWidth,
	uint32_t entropyHeight,
	uint16_t* predictorImage,
	uint32_t predictorWidth,
	uint32_t predictorHeight
){
	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;

	uint8_t* image = new uint8_t[width*height*3];

	RansDecSymbol dsyms[entropyContexts][256];
	for(size_t cont = 0;cont < entropyContexts;cont++){
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[cont][i], tables[cont].cum_freqs[i], tables[cont].freqs[i]);
		}
	}

	RansState rans;
	RansDecInit(&rans, &fileIndex);

	for(size_t i=0;i<width*height;i++){
		uint32_t cumFreq = RansDecGet(&rans, 16);
		uint8_t s;

		size_t tileIndex = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);

		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 0]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 0] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 0]][s], 16);


		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 1]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 1] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 1]][s], 16);
		cumFreq = RansDecGet(&rans, 16);
		for(size_t j=0;j<256;j++){
			if(tables[entropyImage[tileIndex*3 + 2]].cum_freqs[j + 1] > cumFreq){
				s = j;
				break;
			}
		}
		image[i*3 + 2] = s;
		RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex*3 + 2]][s], 16);

	}
	printf("residuals decoded\n");
	colourSub_unfilter_all(
		image,
		range,
		width,
		height,
		colourImage,
		colourWidth,
		colourHeight,
		predictorImage,
		predictorWidth,
		predictorHeight
	);

	return image;
}

#endif //DECODERS_HEADER
