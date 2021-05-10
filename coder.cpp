#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <cmath>
#include <math.h>

#include "rans_byte.h"
#include "file_io.hpp"

// This is just the sample program. All the meat is in rans_byte.h.


struct SymbolStats
{
    uint32_t freqs[256];
    uint32_t cum_freqs[257];

    void count_freqs(uint8_t const* in, size_t nbytes);
    void calc_cum_freqs();
    void normalize_freqs(uint32_t target_total);
};

void SymbolStats::count_freqs(uint8_t const* in, size_t nbytes)
{
    for (int i=0; i < 256; i++)
        freqs[i] = 0;

    for (size_t i=0; i < nbytes; i++)
        freqs[in[i]]++;
}

void SymbolStats::calc_cum_freqs()
{
    cum_freqs[0] = 0;
    for (int i=0; i < 256; i++)
        cum_freqs[i+1] = cum_freqs[i] + freqs[i];
}

void SymbolStats::normalize_freqs(uint32_t target_total)
{
    assert(target_total >= 256);
    
    calc_cum_freqs();
    uint32_t cur_total = cum_freqs[256];
    
    // resample distribution based on cumulative freqs
    for (int i = 1; i <= 256; i++)
        cum_freqs[i] = ((uint64_t)target_total * cum_freqs[i])/cur_total;

    // if we nuked any non-0 frequency symbol to 0, we need to steal
    // the range to make the frequency nonzero from elsewhere.
    //
    // this is not at all optimal, i'm just doing the first thing that comes to mind.
    for (int i=0; i < 256; i++) {
        if (freqs[i] && cum_freqs[i+1] == cum_freqs[i]) {
            // symbol i was set to zero freq

            // find best symbol to steal frequency from (try to steal from low-freq ones)
            uint32_t best_freq = ~0u;
            int best_steal = -1;
            for (int j=0; j < 256; j++) {
                uint32_t freq = cum_freqs[j+1] - cum_freqs[j];
                if (freq > 1 && freq < best_freq) {
                    best_freq = freq;
                    best_steal = j;
                }
            }
            assert(best_steal != -1);

            // and steal from it!
            if (best_steal < i) {
                for (int j = best_steal + 1; j <= i; j++)
                    cum_freqs[j]--;
            } else {
                assert(best_steal > i);
                for (int j = i + 1; j <= best_steal; j++)
                    cum_freqs[j]++;
            }
        }
    }

    // calculate updated freqs and make sure we didn't screw anything up
    assert(cum_freqs[0] == 0 && cum_freqs[256] == target_total);
    for (int i=0; i < 256; i++) {
        if (freqs[i] == 0)
            assert(cum_freqs[i+1] == cum_freqs[i]);
        else
            assert(cum_freqs[i+1] > cum_freqs[i]);

        // calc updated freq
        freqs[i] = cum_freqs[i+1] - cum_freqs[i];
    }
}

uint8_t median3(uint8_t a, uint8_t b, uint8_t c){
	if(a > b){
		if(b > c){
			return b;
		}
		else if(c > a){
			return a;
		}
		else{
			return c;
		}
	}
	else{
		if(b < c){
			return b;
		}
		else if(c > a){
			return c;
		}
		else{
			return a;
		}
	}
}

uint8_t* filter_all_ffv1(uint8_t* in_bytes, uint32_t width, uint32_t height){
	uint8_t* filtered = new uint8_t[width * height];

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - median3(
					L,
					T,
					L + T - TL
				)
			);
		}
	}
	return filtered;
}

uint8_t* filter_all_generic(uint8_t* in_bytes, uint32_t width, uint32_t height,int8_t a,int8_t b,int8_t c,int8_t d){
	uint8_t* filtered = new uint8_t[width * height];
	int8_t sum = a + b + c + d;

	filtered[0] = in_bytes[0];//TL prediction
	for(size_t i=1;i<width;i++){
		filtered[i] = in_bytes[i] - in_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		filtered[y * width] = in_bytes[y * width] - in_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = in_bytes[y * width + i - 1];
			uint8_t TL = in_bytes[(y-1) * width + i - 1];
			uint8_t T = in_bytes[(y-1) * width + i];
			uint8_t TR = in_bytes[(y-1) * width + i + 1];
			filtered[(y * width) + i] = (
				in_bytes[y * width + i] - (
					a*L + b*T + c*TL + d*TR
				)/sum
			);
		}
	}
	return filtered;
}

			/*if(y == 100 && i == 110){
				printf("filtered %d\n",filtered[y * width + i]);
				printf("value %d\n",in_bytes[i]);
				printf("L %d\n",in_bytes[(y) * width + i - 1]);
				printf("T %d\n",in_bytes[(y-1) * width + i]);
				printf("TL %d\n",in_bytes[(y-1) * width + i - 1]);
				printf("median %d\n",median3(
					in_bytes[(y) * width + i - 1],
					in_bytes[(y-1) * width + i],
					in_bytes[(y) * width + i - 1] + in_bytes[(y-1) * width + i] - in_bytes[(y-1) * width + i - 1]
				));
			}*/

double estimateEntropy(uint8_t* in_bytes, size_t size){
	uint8_t frequencies[size];
	for(size_t i=0;i<256;i++){
		frequencies[i] = 0;
	}
	for(size_t i=0;i<size;i++){
		frequencies[in_bytes[i]]++;
	}
	double sum = 0;
	for(size_t i=0;i<256;i++){
		if(frequencies[i]){
			sum += -std::log2((double)frequencies[i]/(double)size) * (double)frequencies[i];
		}
	}
	return sum;
}

const uint32_t prob_bits = 16;
const uint32_t prob_scale = 1 << prob_bits;

class EntropyEncoder{
	public:
		EntropyEncoder();
		~EntropyEncoder();
		void encodeSymbol(RansEncSymbol* magic,uint8_t byte);
		uint8_t* conclude(size_t* streamSize);
		void writeByte(uint8_t byte);
	private:
		static const size_t out_max_size = 32<<20;
		static const size_t out_max_elems = out_max_size / sizeof(uint8_t);
		uint8_t* out_buf;
		uint8_t* out_end;
		uint8_t* ptr;
		uint8_t buffer;
		uint8_t buffer_length;
		RansState rans;
};

EntropyEncoder::EntropyEncoder(void){
	out_buf = new uint8_t[out_max_elems];
	out_end = out_buf + out_max_elems;
	ptr = out_end;
	RansEncInit(&rans);
	buffer = 0;
	buffer_length = 0;
}
EntropyEncoder::~EntropyEncoder(void){
	delete[] out_buf;
}
void EntropyEncoder::encodeSymbol(RansEncSymbol* magic,uint8_t byte){
	RansEncPutSymbol(&rans, &ptr, magic + byte);
}
void EntropyEncoder::writeByte(uint8_t byte){
	*ptr = byte;
	ptr--;
}
uint8_t* EntropyEncoder::conclude(size_t* streamSize){
	RansEncFlush(&rans, &ptr);
	*streamSize = out_end - ptr;
	uint8_t* out = new uint8_t[*streamSize];
	for(size_t i=0;i<*streamSize;i++){
		out[i] = ptr[i];
	}
	return out;
}

class EntropyDecoder{
	public:
		EntropyDecoder(uint8_t* start);
		~EntropyDecoder();
		uint8_t decodeSymbol(RansDecSymbol* magic);
		uint8_t readByte();
		uint8_t readBits(uint8_t number);
	private:
		uint8_t* ptr;
		uint8_t buffer;
		uint8_t buffer_length;
		RansState rans;
};

void binaryPaletteImage(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 0;//use flat entropy table
	for(size_t i=0;i<width*height;i += 8){
		*(outPointer++) =
			(in_bytes[i] << 7)
			 + (in_bytes[i+1] << 6)
			 + (in_bytes[i+2] << 5)
			 + (in_bytes[i+3] << 4)
			 + (in_bytes[i+4] << 3)
			 + (in_bytes[i+5] << 2)
			 + (in_bytes[i+6] << 1)
			 + (in_bytes[i+7]);
	}
	int residual = width*height % 8;
	if(residual){
		*(outPointer++) = 0;//fix later
	}
}

void print_usage(){
	printf("./coder infile.grey width height outfile.hoh speed\n\nspeed is a number from 0-1");
}

void writeVarint(uint32_t value,uint8_t*& outPointer){
	if(value < 128){
		*(outPointer++) = (uint8_t)value;
	}
	else{
		*(outPointer++) = 128 + (uint8_t)(value >> 7);
		*(outPointer++) = (uint8_t)(value % 128);
	}
}

uint8_t* smallPaletteCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t symbolCount,uint8_t* size){
	uint8_t* buffer = new uint8_t[width * height + 100];
	buffer[0] = 0;//don't bother with features yet
	if(symbolCount == 2){
		for(size_t i=0;i*8 < width*height - 7;i++){
			buffer[i] = (in_bytes[i*8] << 7) + (in_bytes[i*8 + 1] << 6) + (in_bytes[i*8 + 2] << 5) + (in_bytes[i*8 + 3] << 4) + (in_bytes[i*8 + 4] << 3) + (in_bytes[i*8 + 5] << 2) + (in_bytes[i*8 + 6] << 1) + (in_bytes[i*8 + 7]);
		}
		//handle waff
	}
	else{
	}
	return buffer;
}

void simpleCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 0;//use flat entropy table

	for(size_t index=0;index < width*height;index++){
		*(outPointer++) = in_bytes[index];
	}
}

void staticCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 8;//use 8bit entropy table

	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height);
	stats.normalize_freqs(1 << 16);

	RansEncSymbol esyms[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats.freqs[i]) >> 8;
		*(outPointer++) = (stats.freqs[i]) % 256;
		RansEncSymbolInit(&esyms[i], stats.cum_freqs[i], stats.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;--index;){
		entropy.encodeSymbol(esyms,in_bytes[index]);
	}
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}

void ffv1Coder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	//use ffv1 predictor
	*(outPointer++) = 0b10000000;

	*(outPointer++) = 16;//use 16bit entropy table

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, width, height);
	

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	stats.normalize_freqs(1 << 16);

	RansEncSymbol esyms[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats.freqs[i]) >> 8;
		*(outPointer++) = (stats.freqs[i]) % 256;
		RansEncSymbolInit(&esyms[i], stats.cum_freqs[i], stats.freqs[i], 16);
	}
	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		entropy.encodeSymbol(esyms,filtered_bytes[index]);
	}
	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] buffer;
}

double* entropyLookup(SymbolStats stats,size_t total){
	double* table = new double[256];
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i] == 0){
			table[i] = -std::log2(1/(double)total);
		}
		else{
			table[i] = -std::log2(stats.freqs[i]/(double)total);
		}
	}
	return table;
}

double regionalEntropy(
	uint8_t* in_bytes,
	double* entropyTable,
	size_t tileIndex,
	uint32_t width,
	uint32_t height,
	uint32_t b_width,
	uint32_t b_height,
	size_t blockSize
){
	double sum = 0;
	for(size_t y=0;y<blockSize;y++){
		uint32_t y_pos = (tileIndex / b_width)*blockSize + y;
		if(y >= height){
			continue;
		}
		for(size_t x=0;x<blockSize;x++){
			uint32_t x_pos = (tileIndex % b_width)*blockSize + x;
			if(x >= width){
				continue;
			}
			sum += entropyTable[in_bytes[y_pos * width + x_pos]];
		}
	}
	return sum;
}

size_t tileIndexFromPixel(
	size_t pixel,
	uint32_t width,
	uint32_t b_width,
	size_t blockSize
){
	uint32_t y = pixel / width;
	uint32_t x = pixel % width;
	uint32_t b_y = y / 8;
	uint32_t b_x = x / 8;
	return b_y * b_width + b_x;
}

void fastCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t*& outPointer){

	//TODO add actual decoding?
	*(outPointer++) = 0b00010000;
	*(outPointer++) = 0b00000000;
	*(outPointer++) = 0b00000000;

	//something something entropy and LZ
	*(outPointer++) = 0b00000000;
	*(outPointer++) = 0b00000000;

	size_t blockSize = 8;
	uint32_t b_width = (width + blockSize - 1)/blockSize;
	uint32_t b_height = (height + blockSize - 1)/blockSize;

	uint8_t* filtered_bytes  = filter_all_ffv1(in_bytes, width, height);
	//uint8_t* filtered_bytes2 = filter_all_generic(in_bytes, width, height,1,2,0,1);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	double* entropyTable = entropyLookup(stats,width*height);

	double entropyMap[b_width*b_height];
	double sum = 0;
	for(size_t i=0;i<b_width*b_height;i++){
		double region = regionalEntropy(
			filtered_bytes,
			entropyTable,
			i,
			width,
			height,
			b_width,
			b_height,
			blockSize
		);
		entropyMap[i] = region;
		sum += region;
	}
	double average = sum/(b_width*b_height);//use for partitioning

	uint8_t* entropyIndex = new uint8_t[b_width*b_height];
	for(size_t i=0;i<b_width*b_height;i++){
		if(entropyMap[i] < average){
			entropyIndex[i] = 0;
		}
		else{
			entropyIndex[i] = 1;
		}
	}

	binaryPaletteImage(entropyIndex,b_width,b_height,outPointer);

	SymbolStats stats1;
	SymbolStats stats2;
	for(size_t i=0;i<256;i++){
		stats1.freqs[i] = 0;
		stats2.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(entropyIndex[tileIndexFromPixel(
			i,
			width,
			b_width,
			blockSize
		)] == 0){
			stats1.freqs[filtered_bytes[i]]++;
		}
		else{
			stats2.freqs[filtered_bytes[i]]++;
		}
	}




	*(outPointer++) = 16;//use 16bit entropy table
	stats1.normalize_freqs(1 << 16);

	RansEncSymbol esyms1[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats1.freqs[i]) >> 8;
		*(outPointer++) = (stats1.freqs[i]) % 256;
		RansEncSymbolInit(&esyms1[i], stats1.cum_freqs[i], stats1.freqs[i], 16);
	}

	*(outPointer++) = 16;//use 16bit entropy table
	stats2.normalize_freqs(1 << 16);

	RansEncSymbol esyms2[256];

	for (int i=0; i < 256; i++) {
		*(outPointer++) = (stats2.freqs[i]) >> 8;
		*(outPointer++) = (stats2.freqs[i]) % 256;
		RansEncSymbolInit(&esyms2[i], stats2.cum_freqs[i], stats2.freqs[i], 16);
	}


	EntropyEncoder entropy;
	for(size_t index=width*height;index--;){
		if(entropyIndex[tileIndexFromPixel(
			index,
			width,
			b_width,
			blockSize
		)] == 0){
			entropy.encodeSymbol(esyms1,filtered_bytes[index]);
		}
		else{
			entropy.encodeSymbol(esyms2,filtered_bytes[index]);
		}
	}
	delete[] filtered_bytes;
	size_t streamSize;
	uint8_t* buffer = entropy.conclude(&streamSize);
	//printf("streamsize %d\n",(int)streamSize);
	for(size_t i=0;i<streamSize;i++){
		*(outPointer++) = buffer[i];
	}
	delete[] entropyIndex;
	delete[] entropyTable;
	delete[] buffer;
}

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	uint32_t width = atoi(argv[2]);
	uint32_t height = atoi(argv[3]);

	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	if(
		width == 0 || height == 0
		|| (width*height) != in_size
	){
		printf("invalid width or height\n");
		print_usage();
		return 2;
	}

	printf("read %d bytes\n",(int)in_size);
	printf("width x height %d x %d\n",(int)(width),(int)(height));

	size_t cruncher_mode = 0;
	if(argc > 4 && strcmp(argv[5],"0") == 0){
		cruncher_mode = 0;
	}
	else if(argc > 4 && strcmp(argv[5],"1") == 0){
		cruncher_mode = 1;
	}
	else if(argc > 4){
		printf("invalid speed setting\n");
		print_usage();
	}

	uint8_t* out_buf = new uint8_t[32<<20];
	uint8_t* outPointer = out_buf;
	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);
	//simpleCoder(in_bytes,width,height,out_buf,outPointer);
	//staticCoder(in_bytes,width,height,out_buf,outPointer);
	if(cruncher_mode == 0){
		ffv1Coder(in_bytes,width,height,out_buf,outPointer);
	}
	else if(cruncher_mode == 1){
		fastCoder(in_bytes,width,height,out_buf,outPointer);
	}
	delete[] in_bytes;

	printf("file size %d\n",(int)(outPointer - out_buf));


	write_file(argv[4],out_buf,outPointer - out_buf);
	delete[] out_buf;
	return 0;
}
