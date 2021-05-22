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
#include "2dutils.hpp"
#include "varint.hpp"
#include "lode_io.hpp"
#include "laplace.hpp"
#include "panic.hpp"

void print_usage(){
	printf("./dhoh infile.hoh outfile.png\n");
}

void unfilter_all_ffv1(uint8_t* in_bytes, uint32_t width, uint32_t height){
	for(size_t i=1;i<width;i++){
		in_bytes[i] = in_bytes[i] + in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		in_bytes[y * width] = in_bytes[y * width] + in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			in_bytes[(y * width) + i] = (
				in_bytes[y * width + i] + median3(
					L,
					T,
					L + T - TL
				)
			);
		}
	}
}

class BitReader{
	public:
		BitReader(uint8_t** source);
		BitReader(uint8_t** source,uint8_t startBits,uint8_t startBits_number);
		uint8_t readBits(uint8_t size);
	private:
		uint8_t partial;
		uint8_t partial_length;
		uint8_t** byteSource;
};

BitReader::BitReader(uint8_t** source){
	partial = 0;
	partial_length = 0;
	byteSource = source;
}

BitReader::BitReader(uint8_t** source,uint8_t startBits,uint8_t startBits_number){
	partial = startBits;
	partial_length = startBits_number;
	byteSource = source;
}

uint8_t BitReader::readBits(uint8_t size){
	if(size == partial_length){
		uint8_t result = partial;
		partial = 0;
		partial_length = 0;
		return result;
	}
	else if(size < partial_length){
		uint8_t result = partial >> (partial_length - size);
		partial = partial - (result << (partial_length - size));
		partial_length -= size;
		return result;
	}
	else{
		uint8_t result = partial << (size - partial_length);
		size -= partial_length;
		partial = **byteSource;
		(*byteSource)++;
		partial_length = 8;

		result += partial >> (partial_length - size);
		partial = partial - (result << (partial_length - size));
		partial_length -= size;

		return result;
	}
}

SymbolStats decode_freqTable(BitReader reader){
	SymbolStats stats;
	uint8_t mode = reader.readBits(1);
	if(mode == 1){//special modes
		mode = reader.readBits(7);
		if(mode == 0){
			for(size_t i=0;i<256;i++){
				stats.freqs[i] = 256;
			}
		}
		else{
			return laplace(mode);
		}
	}
	else{
	}
	return stats;
}

uint8_t* read_ranged_greyscale(uint8_t*& fileIndex,size_t range,uint32_t width,uint32_t height);

uint8_t* read_binary_greyscale(uint8_t*& fileIndex,uint32_t width,uint32_t height){
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

	uint8_t* bitmap;
	if(
		SYMMETRY == 0
		&& INDEX == 0
		&& PREDICTION == 0
		&& ENTROPY_MAP == 0
		&& LZ == 0
		&& CODED == 0
	){
		bitmap = new uint8_t[width*height];
		for(size_t i=0;i<width*height;i += 8){
			uint8_t bits = *(fileIndex++);
			bitmap[i*8] = (bits & 0b10000000) >> 7;
			if(i+1 < width*height){
				bitmap[i*8 + 1] = (bits & 0b01000000) >> 6;
			}
			if(i+2 < width*height){
				bitmap[i*8 + 2] = (bits & 0b00100000) >> 5;
			}
			if(i+3 < width*height){
				bitmap[i*8 + 3] = (bits & 0b00010000) >> 4;
			}
			if(i+4 < width*height){
				bitmap[i*8 + 4] = (bits & 0b00001000) >> 3;
			}
			if(i+5 < width*height){
				bitmap[i*8 + 5] = (bits & 0b00000100) >> 2;
			}
			if(i+6 < width*height){
				bitmap[i*8 + 6] = (bits & 0b00000010) >> 1;
			}
			if(i+7 < width*height){
				bitmap[i*8 + 7] = (bits & 0b00000001) >> 0;
			}
		}
	}
	return bitmap;
}

uint8_t* read_8bit_greyscale(uint8_t*& fileIndex,uint32_t width,uint32_t height){
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
		indexImage = read_8bit_greyscale(fileIndex,indexWidth,1);
	}
	uint8_t predictors[235];
	size_t predictorCount = 0;
	uint8_t* predictorImage;
	if(PREDICTION){
		printf("using prediction\n");
		uint8_t predictionMode = *(fileIndex++);
		if(predictionMode < 200){//fix number later
			predictorCount = 1;
			predictors[0] = predictionMode;
		}

		if(predictorCount > 1){
			printf("  %d predictors\n",(int)predictorCount);
			predictorImage = read_ranged_greyscale(fileIndex,predictorCount,indexWidth,1);
		}
		else{
			printf("  1 predictor\n");
		}
	}

	uint8_t entropyContexts = 1;
	

	SymbolStats tables[entropyContexts];
	if(CODED){
		printf("using ANS entropy coding\n");
		printf("  %d entropy tables\n",(int)entropyContexts);
		BitReader reader(&fileIndex);
		printf("  reader created\n");
		for(size_t i=0;i<entropyContexts;i++){
			printf("    table read\n");
			tables[i] = decode_freqTable(reader);
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
		for(size_t i=0;i<width*height;i++){
			bitmap[i] = *(fileIndex++);
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

		unfilter_all_ffv1(bitmap, width, height);
	}
	else{
		printf("missing decoder functionallity! writing black image\n");
		for(size_t i=0;i<width*height;i++){
			bitmap[i] = 0;
		}
	}
	return bitmap;
}

uint8_t* read_ranged_greyscale(uint8_t*& fileIndex,size_t range,uint32_t width,uint32_t height){
	if(range == 2){
		return read_binary_greyscale(fileIndex,width,height);
	}
	else if(range == 256){
		return read_8bit_greyscale(fileIndex,width,height);
	}
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
		return bitmap_expander(read_8bit_greyscale(fileIndex,width,height),width,height);
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
