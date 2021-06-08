#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "unfilters.hpp"
#include "2dutils.hpp"
#include "varint.hpp"
#include "lode_io.hpp"
#include "laplace.hpp"
#include "panic.hpp"
#include "numerics.hpp"
#include "bitreader.hpp"
#include "table_decode.hpp"

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

uint8_t* read_ranged_greyscale(uint8_t*& fileIndex,size_t range,uint32_t width,uint32_t height){
	uint8_t compressionMode = *(fileIndex++);

	uint8_t PREDICTION_MAP  = (compressionMode & 0b00000100) >> 2;
	uint8_t ENTROPY_MAP = (compressionMode & 0b00000010) >> 1;
	uint8_t LZ          = (compressionMode & 0b00000001);
	if(compressionMode > 7){
		panic("reserved bit set! not a valid hoh file\n");
	}
	uint16_t predictors[256];
	size_t predictorCount = 0;
	uint16_t* predictorImage;
	uint32_t predictorWidth = 1;
	uint32_t predictorHeight = 1;
	if(PREDICTION_MAP){
		printf("  uses prediction\n");
		predictorCount = (*(fileIndex++)) + 1;
		for(size_t i=0;i<predictorCount;i++){
			uint16_t value = ((*(fileIndex++)) << 8);
			value += *(fileIndex++);
			predictors[i] = value;
		}
		printf("  %d predictors\n",(int)predictorCount);
		if(predictorCount > 1){
			uint8_t* trailing = fileIndex;
			predictorWidth = readVarint(fileIndex) + 1;
			predictorHeight = readVarint(fileIndex) + 1;
			printf("---\n");
			printf("  predictor image %d x %d\n",(int)predictorWidth,(int)predictorHeight);
			uint8_t* predictorImage_data = read_ranged_greyscale(fileIndex,predictorCount,predictorWidth,predictorHeight);
			printf("---predictor image size: %d bytes\n",(int)(fileIndex - trailing));
			predictorImage = new uint16_t[predictorWidth*predictorHeight];
			for(size_t i=0;i<predictorWidth*predictorHeight;i++){
				predictorImage[i] = predictors[predictorImage_data[i]];
			}
			delete[] predictorImage_data;
		}
	}
	printf(" starting entropy\n");

	uint8_t entropyContexts = 1;
	uint8_t* entropyImage;
	uint32_t entropyWidth;
	uint32_t entropyHeight;
	uint32_t entropyWidth_block;
	uint32_t entropyHeight_block;
	if(ENTROPY_MAP){
		printf("  has entropy map\n");
		entropyContexts = *(fileIndex++) + 1;
		if(entropyContexts > 1){
			printf("  %d entropy contexts\n",(int)entropyContexts);
			uint8_t* trailing = fileIndex;
			entropyWidth = readVarint(fileIndex) + 1;
			entropyHeight = readVarint(fileIndex) + 1;
			entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
			entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
			printf("---\n");
			printf("  entropy image %d x %d\n",(int)entropyWidth,(int)entropyHeight);
			entropyImage = read_ranged_greyscale(fileIndex,entropyContexts,entropyWidth,entropyHeight);
			printf("---entropy image size: %d bytes\n",(int)(fileIndex - trailing));
		}
	}

	if(LZ){
		panic("LZ decoding not yet implemented!\n");
	}

	uint8_t* trailing = fileIndex;

	BitReader reader(&fileIndex);
	SymbolStats tables[entropyContexts];
	uint8_t blocking[entropyContexts];
	for(size_t i=0;i<entropyContexts;i++){
		tables[i] = decode_freqTable(reader,range,blocking[i]);
		if(blocking[i]){
			panic("entropy blocking not yet implemented!\n");
		}
	}
	printf("  entropy table size: %d bytes\n",(int)(fileIndex - trailing));

	uint8_t* bitmap = new uint8_t[width*height];
	if(
		PREDICTION_MAP == 1 && predictorCount == 1 && predictors[0] == 0
		&& ENTROPY_MAP == 0
		&& LZ == 0
	){
		printf("ransdec ffv1\n");

		RansDecSymbol dsyms[256];
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[i], tables[0].cum_freqs[i], tables[0].freqs[i]);
		}

		RansState rans;
		RansDecInit(&rans, &fileIndex);

		for(size_t i=0;i<width*height;i++){
			uint32_t cumFreq = RansDecGet(&rans, 16);
			uint8_t s;
			for(size_t j=0;j<256;j++){
				if(tables[0].cum_freqs[j + 1] > cumFreq){
					s = j;
					break;
				}
			}
			bitmap[i] = s;
			RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
		}

		unfilter_all_ffv1(bitmap, range, width, height);
	}
	else if(
		PREDICTION_MAP == 1
		&& ENTROPY_MAP == 1
		&& LZ == 0
	){
		printf("ransdec both\n");

		RansDecSymbol dsyms[entropyContexts][256];
		for(size_t context=0;context < entropyContexts;context++){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dsyms[context][i], tables[context].cum_freqs[i], tables[context].freqs[i]);
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
			bitmap[i] = s;
			RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);
		}

		if(predictorCount == 1){
			unfilter_all(
				bitmap,
				range,
				width,
				height,
				predictors[0]
			);
		}
		else{
			unfilter_all(
				bitmap,
				range,
				width,
				height,
				predictorImage,
				predictorWidth,
				predictorHeight
			);
		}
	}
	else if(
		PREDICTION_MAP == 1
		&& ENTROPY_MAP == 0
		&& LZ == 0
	){
		printf("ransdec prediction only\n");
		RansDecSymbol dsyms[256];
		for(size_t i=0;i<256;i++){
			RansDecSymbolInit(&dsyms[i], tables[0].cum_freqs[i], tables[0].freqs[i]);
		}

		RansState rans;
		RansDecInit(&rans, &fileIndex);

		for(size_t i=0;i<width*height;i++){
			uint32_t cumFreq = RansDecGet(&rans, 16);
			uint8_t s;

			for(size_t j=0;j<256;j++){
				if(tables[0].cum_freqs[j + 1] > cumFreq){
					s = j;
					break;
				}
			}
			bitmap[i] = s;
			RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[s], 16);
		}
		if(predictorCount == 1){
			unfilter_all(
				bitmap,
				range,
				width,
				height,
				predictors[0]
			);
		}
		else{
			unfilter_all(
				bitmap,
				range,
				width,
				height,
				predictorImage,
				predictorWidth,
				predictorHeight
			);
		}
	}
	else if(
		PREDICTION_MAP == 0
		&& ENTROPY_MAP == 1
		&& LZ == 0
	){
		printf("ransdec entropy only\n");
		RansDecSymbol dsyms[entropyContexts][256];
		for(size_t context=0;context < entropyContexts;context++){
			for(size_t i=0;i<256;i++){
				RansDecSymbolInit(&dsyms[context][i], tables[context].cum_freqs[i], tables[context].freqs[i]);
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
			bitmap[i] = s;
			RansDecAdvanceSymbol(&rans, &fileIndex, &dsyms[entropyImage[tileIndex]][s], 16);
		}
	}
	else{
		printf("missing decoder functionallity! writing black image\n");
		for(size_t i=0;i<width*height;i++){
			bitmap[i] = 0;
		}
	}
	if(predictorCount > 1){
		delete[] predictorImage;
	}
	if(entropyContexts > 1){
		delete[] entropyImage;
	}
	return bitmap;

}

uint8_t* readImage(uint8_t*& fileIndex,uint32_t width,uint32_t height){
	uint8_t* normal = read_ranged_greyscale(fileIndex,256,width,height);
	uint8_t* expanded = bitmap_expander(normal,width,height);
	delete[] normal;
	return expanded;
}

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}
	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	printf("read %d bytes\n",(int)in_size);

	uint8_t* fileIndex = in_bytes;
	uint32_t width =    readVarint(fileIndex) + 1;
	uint32_t height =   readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* out_bytes = readImage(fileIndex,width,height);
	
	delete[] in_bytes;

	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height*4;i++){
		image[i] = out_bytes[i];
	}

	delete[] out_bytes;

	encodeOneStep(argv[2], image, width, height);

	return 0;
}
