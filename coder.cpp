#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

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
//	delete[] out_buf;
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

void print_usage(){
	printf("./coder infile.grey width height outfile.hoh\n");
}

void writeVarint(uint32_t value,uint8_t* outPointer){

	*outPointer = (uint8_t)value;
}

void simpleCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t* outPointer){
	*(outPointer++) = 0;//use no features
	*(outPointer++) = 0;//use flat entropy table
	RansEncSymbol esyms[256];

	for (int i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], i*256, 256, 16);
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

void staticCoder(uint8_t* in_bytes,uint32_t width,uint32_t height,uint8_t* out_buf,uint8_t* outPointer){
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

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	uint32_t width = atoi(argv[2]);
	uint32_t height = atoi(argv[3]);
	if(width == 0 || height == 0){
		printf("invalid width or height\n");
		print_usage();
		return 2;
	}

	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	printf("read %d bytes\n",(int)in_size);
	printf("width*height %d\n",(int)(width*height));

	uint8_t* out_buf = new uint8_t[32<<20];
	uint8_t* outPointer = out_buf;
	*(outPointer++) = 72;
	*(outPointer++) = 79;
	*(outPointer++) = 72;
	writeVarint((uint32_t)(width - 1), outPointer);
	writeVarint((uint32_t)(height - 1),outPointer);
	*(outPointer++) = 0;
	*(outPointer++) = 0b01000000;
	if(width > 128 || height > 128){
		*(outPointer++) = 0;//no tiles yet
	}
	//simpleCoder(in_bytes,width,height,out_buf,outPointer);
	staticCoder(in_bytes,width,height,out_buf,outPointer);
	delete[] in_bytes;

	write_file(argv[4],out_buf,outPointer - out_buf);
	delete[] out_buf;
	return 0;
}
