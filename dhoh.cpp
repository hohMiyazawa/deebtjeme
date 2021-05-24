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

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

SymbolStats decode_freqTable(BitReader& reader,size_t range){
	SymbolStats stats;
	uint8_t mode = reader.readBits(1);
	if(mode == 1){//special modes
		printf("    laplace table\n");
		mode = reader.readBits(7);
		if(mode == 0){
			for(size_t i=0;i<256;i++){
				if(i < range){
					stats.freqs[i] = 1;
				}
				else{
					stats.freqs[i] = 0;
				}
			}
			stats.normalize_freqs(1 << 16);
		}
		else{
			return laplace(mode);
		}
	}
	else{
		mode = reader.readBits(2);
		if(mode == 3){
			printf("    complete 4bit magnitude table\n");
			for(size_t i=0;i<256;i++){
				if(i >= range){
					stats.freqs[i] = 0;
					continue;
				}
				uint8_t magnitude = reader.readBits(4);
				if(magnitude == 0){
					stats.freqs[i] = 0;
					uint8_t remainingNeededBits = log2_plus(range - 1 - i);
					uint8_t runLength = reader.readBits(remainingNeededBits);
					for(size_t j=0;j<runLength;j++){
						i++;
						stats.freqs[i] = 0;
					}
				}
				else{
					uint32_t power = 1 << (magnitude - 1);
					stats.freqs[i] = power;
					if(magnitude == 15){
						if(reader.readBits(1)){
							magnitude = 16;
							stats.freqs[i] *= 2;
						}
					}
					
					uint8_t extraBits = magnitude - 1;
					if(extraBits){
						if(extraBits > 8){
							stats.freqs[i] += ((uint32_t)reader.readBits(extraBits - 8)) << 8;
							stats.freqs[i] += reader.readBits(8);
						}
						else{
							stats.freqs[i] += reader.readBits(extraBits);
						}
					}
				}
			}
		}
		else if(mode == 2){
			printf("    4bit magnitude table\n");
			for(size_t i=0;i<256;i++){
				if(i >= range){
					stats.freqs[i] = 0;
					continue;
				}
				uint8_t magnitude = reader.readBits(4);
				if(magnitude == 0){
					stats.freqs[i] = 0;
					//TODO zero modelling
				}
				else{
					uint32_t power = 1 << (magnitude - 1);
					stats.freqs[i] = power;
					uint8_t extraBits = magnitude >> 1;
					if(extraBits){
						extraBits--;
						stats.freqs[i] += reader.readBits(extraBits) << (magnitude - extraBits - 1);
					}
				}
			}
		}
		else{
			panic("only 4bit magnitude coding implemented!\n");
		}
/*
		for(size_t i=0;i<256;i++){
			printf("table %d %d\n",(int)i,(int)stats.freqs[i]);
		}
*/
		stats.normalize_freqs(1 << 16);
	}
	//printf("    table read\n");
	return stats;
}

uint8_t* read_ranged_greyscale(uint8_t*& fileIndex,size_t range,uint32_t width,uint32_t height){
	uint8_t compressionMode = *(fileIndex++);

	uint8_t RESERVED    = (compressionMode & 0b10000000) >> 7;
	uint8_t SYMMETRY    = (compressionMode & 0b01000000) >> 6;
	uint8_t SUB_GREEN   = (compressionMode & 0b00100000) >> 5;
	uint8_t INDEX       = (compressionMode & 0b00010000) >> 4;

	uint8_t PREDICTION  = (compressionMode & 0b00001000) >> 3;
	uint8_t ENTROPY_MAP = (compressionMode & 0b00000100) >> 2;
	uint8_t LZ          = (compressionMode & 0b00000010) >> 1;
	uint8_t CODED       = (compressionMode & 0b00000001) >> 0;
	if(RESERVED == 1){
		panic("reserved bit set! not a valid hoh file\n");
	}
	if(SUB_GREEN == 1){
		panic("SUB_GREEN transform not valid for greyscale images!\n");
	}

	uint8_t symmetryMode = 0;
	if(SYMMETRY){
		printf("using symmetry modes\n");
		symmetryMode = *(fileIndex++);
	}
	uint8_t* indexImage;
	uint32_t indexWidth;
	if(INDEX){
		printf("using colour index\n");
		indexWidth = readVarint(fileIndex) + 1;
		indexImage = read_ranged_greyscale(fileIndex,range,indexWidth,1);
	}
	uint8_t predictors[235];
	size_t predictorCount = 0;
	uint8_t* predictorImage;
	uint32_t predictorWidth = 1;
	uint32_t predictorHeight = 1;
	if(PREDICTION){
		printf("using prediction\n");
		uint8_t predictionMode = *(fileIndex++);
		if(predictionMode < 226){
			predictorCount = 1;
			predictors[0] = predictionMode;
			printf("  predictor: %d\n",(int)predictors[0]);
		}
		else if(predictionMode < 255){
			predictorCount = 255 - predictionMode;
			for(size_t i=0;i<predictorCount;i++){
				predictors[i] = *(fileIndex++);
			}
		}
		else{
			//TODO read bitmask
		}

		if(predictorCount > 1){
			printf("  %d predictors\n",(int)predictorCount);
			predictorWidth = readVarint(fileIndex) + 1;
			predictorHeight = readVarint(fileIndex) + 1;
			printf("  predictor image %d x %d\n",(int)predictorWidth,(int)predictorHeight);
			printf("---\n");
			predictorImage = read_ranged_greyscale(fileIndex,predictorCount,predictorWidth,predictorHeight);
			printf("--- end predictor image\n");
			for(size_t i=0;i<predictorWidth*predictorHeight;i++){
				predictorImage[i] = predictors[predictorImage[i]];
			}
		}
		else{
			printf("  1 predictor\n");
		}
	}

	uint8_t entropyContexts = 1;
	uint8_t* entropyImage;
	uint32_t entropyWidth;
	uint32_t entropyHeight;
	uint32_t entropyWidth_block;
	uint32_t entropyHeight_block;
	if(ENTROPY_MAP){
		entropyContexts = *(fileIndex++) + 1;
		if(entropyContexts > 1){
			printf("  %d entropy contexts\n",(int)entropyContexts);
			entropyWidth = readVarint(fileIndex) + 1;
			entropyHeight = readVarint(fileIndex) + 1;
			entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
			entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
			printf("  entropy image %d x %d\n",(int)entropyWidth,(int)entropyHeight);
			entropyImage = read_ranged_greyscale(fileIndex,entropyContexts,entropyWidth,entropyHeight);
		}
	}

	if(LZ){
		panic("LZ decoding not yet implemented!\n");
	}
/*
	printf("    bbuffer content %d\n",(int)(*(fileIndex-3)));
	printf("    bbuffer content %d\n",(int)(*(fileIndex-2)));
	printf("    bbuffer content %d\n",(int)(*(fileIndex-1)));
*/

	SymbolStats tables[entropyContexts];
	if(CODED){
		printf("using ANS entropy coding\n");
		printf("  %d entropy tables\n",(int)entropyContexts);
		BitReader reader(&fileIndex);
		printf("  reader created\n");
		for(size_t i=0;i<entropyContexts;i++){
			tables[i] = decode_freqTable(reader,range);
		}
	}

	uint8_t* bitmap = new uint8_t[width*height];
	if(
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 0
		&& ENTROPY_MAP == 0
		&& LZ == 0
		&& CODED == 0
	){
		if(range == 256){
			for(size_t i=0;i<width*height;i++){
				bitmap[i] = *(fileIndex++);
			}
		}
		else{
			BitReader reader(&fileIndex);
			uint8_t bitsPerSymbol = log2_plus(range - 1);
			for(size_t i=0;i<width*height;i++){
				bitmap[i] = reader.readBits(bitsPerSymbol);
			}
		}
	}
	else if(
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 1 && predictorCount == 1 && predictors[0] == 0
		&& ENTROPY_MAP == 0
		&& LZ == 0
		&& CODED == 1
	){
		printf("ransdec\n");

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
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 1 && predictorCount == 1 && predictors[0] == 68
		&& ENTROPY_MAP == 0
		&& LZ == 0
		&& CODED == 1
	){
		printf("ransdec\n");

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

		unfilter_all_left(bitmap, range, width, height);
	}
	else if(
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 1
		&& ENTROPY_MAP == 1 && entropyContexts > 1
		&& LZ == 0
		&& CODED == 1
	){
		printf("ransdec\n");

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
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 1 && predictorCount > 1
		&& ENTROPY_MAP == 0 && entropyContexts == 1
		&& LZ == 0
		&& CODED == 1
	){
		printf("ransdec\n");
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
	else{
		printf("missing decoder functionallity! writing black image\n");
		for(size_t i=0;i<width*height;i++){
			bitmap[i] = 0;
		}
	}
	if(INDEX){
		delete[] indexImage;
	}
	if(predictorCount > 1){
		delete[] predictorImage;
	}
	if(entropyContexts > 1){
		delete[] entropyImage;
	}
	return bitmap;

}

uint8_t* bitmap_expander(uint8_t* bitmap,uint32_t width,uint32_t height){
	uint8_t* bitmap_expanded = new uint8_t[width*height*4];
	for(size_t i=0;i<width*height;i++){
		bitmap_expanded[i*4 + 0] = bitmap[i];
		bitmap_expanded[i*4 + 1] = bitmap[i];
		bitmap_expanded[i*4 + 2] = bitmap[i];
		bitmap_expanded[i*4 + 3] = 255;
	}
	delete[] bitmap;
	return bitmap_expanded;
}

uint8_t* readImage(uint8_t*& fileIndex,uint32_t width,uint32_t height){
	uint8_t imageMode = *(fileIndex++);
	uint8_t reservedBit = imageMode >> 7;
	if(reservedBit == 1){
		panic("reserved bit set! not a valid hoh file\n");
	}
	uint8_t bitDepth = ((imageMode & 0b01111100) >> 2) + 1;
	if(bitDepth != 8){
		panic("bit depths other than 8 not yet implemented!\n");
	}
	uint8_t hasAlpha  = (imageMode & 0b00000010) >> 1;
	uint8_t hasColour = (imageMode & 0b00000001);
	if(hasAlpha){
		panic("alpha decoding not yet implemented!\n");
	}
	if(hasColour){
		panic("colour decoding not yet implemented!\n");
	}
	if(!hasAlpha && !hasColour && bitDepth == 8){
		return bitmap_expander(read_ranged_greyscale(fileIndex,256,width,height),width,height);
	}
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
