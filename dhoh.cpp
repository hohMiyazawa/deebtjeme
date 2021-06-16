#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "file_io.hpp"
#include "2dutils.hpp"
#include "varint.hpp"
#include "lode_io.hpp"
#include "decoders.hpp"
#include "symbolstats.hpp"
#include "table_decode.hpp"

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

void sanity_check(
	bool RESERVED,
	bool PROGRESSIVE,
	bool HAS_COLOUR,
	bool COLOUR_TRANSFORM,
	bool INDEX_TRANSFORM,
	bool PREDICTION_MAP,
	bool ENTROPY_MAP,
	bool LZ
){
	if(RESERVED){
		panic("RESERVED bit set!\n");
	}
	if(!HAS_COLOUR && COLOUR_TRANSFORM){
		panic("Can not use colour transform for greyscale!\n");
	}
	if(
		!ENTROPY_MAP
		&& (
			COLOUR_TRANSFORM
			|| PREDICTION_MAP
		)
	){
		panic("Nonsensical features without entropy coding!\n");
	}
}

uint8_t* readImage(uint8_t*& fileIndex, size_t range,uint32_t width,uint32_t height){
	uint8_t compressionMode = *(fileIndex++);

	bool RESERVED        = (compressionMode & 0b10000000) >> 7;
	bool PROGRESSIVE     = (compressionMode & 0b01000000) >> 6;
	bool HAS_COLOUR      = (compressionMode & 0b00100000) >> 5;
	bool COLOUR_TRANSFORM= (compressionMode & 0b00010000) >> 4;
	bool INDEX_TRANSFORM = (compressionMode & 0b00001000) >> 3;
	bool PREDICTION_MAP  = (compressionMode & 0b00000100) >> 2;
	bool ENTROPY_MAP     = (compressionMode & 0b00000010) >> 1;
	bool LZ              = (compressionMode & 0b00000001) >> 0;

	sanity_check(
		RESERVED,
		PROGRESSIVE,
		HAS_COLOUR,
		COLOUR_TRANSFORM,
		INDEX_TRANSFORM,
		PREDICTION_MAP,
		ENTROPY_MAP,
		LZ
	);
	uint32_t colourWidth = 1;
	uint32_t colourHeight = 1;
	uint32_t colourWidth_block;
	uint32_t colourHeight_block;
	uint8_t* colourImage;
	if(COLOUR_TRANSFORM){
		uint8_t* trailing = fileIndex;
		colourWidth = readVarint(fileIndex) + 1;
		colourHeight = readVarint(fileIndex) + 1;
		colourWidth_block  = (width + colourWidth - 1)/colourWidth;
		colourHeight_block = (height + colourHeight - 1)/colourHeight;
		printf("---\n");
		printf("  colour transform image %d x %d\n",(int)colourWidth,(int)colourHeight);
		colourImage = readImage(fileIndex, 256,colourWidth,colourHeight);
		printf("---colour transform image size: %d bytes\n",(int)(fileIndex - trailing));
	}

	uint16_t predictors[256];
	uint8_t predictorCount = 0;
	uint16_t* predictorImage;
	uint32_t predictorWidth = 1;
	uint32_t predictorHeight = 1;
	uint32_t predictorWidth_block;
	uint32_t predictorHeight_block;
	if(PREDICTION_MAP){
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
			uint8_t peekMode = *fileIndex;
			predictorWidth_block  = (width + predictorWidth - 1)/predictorWidth;
			predictorHeight_block = (height + predictorHeight - 1)/predictorHeight;
			printf("---\n");
			printf("  predictor image %d x %d\n",(int)predictorWidth,(int)predictorHeight);
			uint8_t* predictorImage_data = readImage(fileIndex,predictorCount,predictorWidth,predictorHeight);
			printf("---predictor image size: %d bytes\n",(int)(fileIndex - trailing));
			if(HAS_COLOUR && (peekMode & 0b00100000)){
				predictorImage = new uint16_t[predictorWidth*predictorHeight*3];
				for(size_t i=0;i<predictorWidth*predictorHeight*3;i++){
					predictorImage[i] = predictors[predictorImage_data[i]];
				}
				delete[] predictorImage_data;
			}
			else if(HAS_COLOUR){
				predictorImage = new uint16_t[predictorWidth*predictorHeight*3];
				for(size_t i=0;i<predictorWidth*predictorHeight;i++){
					predictorImage[i*3 + 0] = predictors[predictorImage_data[i]];
					predictorImage[i*3 + 1] = predictors[predictorImage_data[i]];
					predictorImage[i*3 + 2] = predictors[predictorImage_data[i]];
				}
				delete[] predictorImage_data;
			}
			else{
				predictorImage = new uint16_t[predictorWidth*predictorHeight];
				for(size_t i=0;i<predictorWidth*predictorHeight;i++){
					predictorImage[i] = predictors[predictorImage_data[i]];
				}
				delete[] predictorImage_data;
			}
		}
	}

	uint8_t entropyContexts = 0;
	uint8_t* entropyImage;
	uint32_t entropyWidth;
	uint32_t entropyHeight;
	uint32_t entropyWidth_block;
	uint32_t entropyHeight_block;
	if(ENTROPY_MAP){
		entropyContexts = *(fileIndex++) + 1;
		if(entropyContexts > 1){
			printf("  %d entropy contexts\n",(int)entropyContexts);
			uint8_t* trailing = fileIndex;
			entropyWidth = readVarint(fileIndex) + 1;
			entropyHeight = readVarint(fileIndex) + 1;
			uint8_t peekMode = *fileIndex;
			entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
			entropyHeight_block = (height + entropyHeight - 1)/entropyHeight;
			printf("---\n");
			printf("  entropy image %d x %d\n",(int)entropyWidth,(int)entropyHeight);
			if(
				(HAS_COLOUR && (peekMode & 0b00100000))
				|| !HAS_COLOUR
			){
				entropyImage = readImage(fileIndex,entropyContexts,entropyWidth,entropyHeight);
			}
			else{
				uint8_t* buffer = readImage(fileIndex,entropyContexts,entropyWidth,entropyHeight);
				entropyImage = bitmap_expander(buffer,entropyWidth,entropyHeight);
				delete[] buffer;
			}
			printf("---entropy image size: %d bytes\n",(int)(fileIndex - trailing));
		}
	}

	if(LZ){
		//no extra data in the varint model
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
	if(entropyContexts){
		printf("  entropy table size: %d bytes\n",(int)(fileIndex - trailing));
	}

	if(INDEX_TRANSFORM){
		panic("index transform not yet implemented!\n");
	}
	if(PROGRESSIVE){
		panic("progressive decoding not yet implemented!\n");
	}
	if(HAS_COLOUR){
		if(
			ENTROPY_MAP == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_raw_colour(fileIndex, 256, width, height);
		}
		else if(
			ENTROPY_MAP == true && entropyContexts == 1
			&& PREDICTION_MAP == 0
			&& COLOUR_TRANSFORM == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_entropy_colour(fileIndex, 256, width, height, tables[0]);
		}
		else if(
			ENTROPY_MAP == true
			&& PREDICTION_MAP == 0
			&& COLOUR_TRANSFORM == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_entropyMap_colour(fileIndex, 256, width, height, tables, entropyImage, entropyContexts, entropyWidth, entropyHeight);
		}
		else if(
			ENTROPY_MAP == true && entropyContexts == 1
			&& PREDICTION_MAP == true && predictorCount == 1
			&& COLOUR_TRANSFORM == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_entropy_prediction_colour(fileIndex, 256, width, height, tables[0], predictors[0]);
		}
		else if(
			ENTROPY_MAP == true
			&& PREDICTION_MAP == true && predictorCount == 1
			&& COLOUR_TRANSFORM == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_entropyMap_prediction_colour(fileIndex, 256, width, height, tables, entropyImage, entropyContexts, entropyWidth, entropyHeight, predictors[0]);
		}
		else if(
			ENTROPY_MAP == true
			&& PREDICTION_MAP == true && predictorCount == 1
			&& COLOUR_TRANSFORM == true
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			printf("colMap entMap pred=1\n");
			return decode_colourMap_entropyMap_prediction_colour(
				fileIndex,
				range,
				width,
				height,
				colourImage,
				colourWidth,
				colourHeight,
				tables,
				entropyImage,
				entropyContexts,
				entropyWidth,
				entropyHeight,
				predictors[0]
			);
		}
		else if(
			ENTROPY_MAP == true
			&& PREDICTION_MAP == true
			&& COLOUR_TRANSFORM == true
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			printf("colMap entMap predMap\n");
			return decode_colourMap_entropyMap_predictionMap_colour(
				fileIndex,
				range,
				width,
				height,
				colourImage,
				colourWidth,
				colourHeight,
				tables,
				entropyImage,
				entropyContexts,
				entropyWidth,
				entropyHeight,
				predictorImage,
				predictorWidth,
				predictorHeight
			);
		}
		else{
			panic("decoder not capable!\n");
		}
	}
	else{
		if(
			ENTROPY_MAP == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_raw(fileIndex, 256, width, height);
		}
		else if(
			ENTROPY_MAP == true && entropyContexts == 1
			&& PREDICTION_MAP == 0
			&& INDEX_TRANSFORM == 0
			&& LZ == 0
		){
			return decode_entropy(fileIndex, 256, width, height, tables[0]);
		}
		else{
			panic("decoder not capable!\n");
		}
	}

	if(predictorCount > 1){
		delete[] predictorImage;
	}
	if(entropyContexts > 1){
		delete[] entropyImage;
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
	uint32_t width =  readVarint(fileIndex) + 1;
	uint32_t height = readVarint(fileIndex) + 1;
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t peekMode = *fileIndex;
	bool HAS_COLOUR  = (peekMode & 0b00100000) >> 5;
	uint8_t* expanded;
	uint8_t* normal = readImage(fileIndex, 256, width, height);
	delete[] in_bytes;
	if(HAS_COLOUR){
		expanded = alpha_expander(normal,width,height);
	}
	else{
		expanded = full_expander(normal,width,height);
	}
	delete[] normal;

	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for(size_t i=0;i<width*height*4;i++){
		image[i] = expanded[i];
	}

	delete[] expanded;

	encodeOneStep(argv[2], image, width, height);

	return 0;
}
