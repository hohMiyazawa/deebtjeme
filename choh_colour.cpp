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
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "filters.hpp"
#include "2dutils.hpp"
#include "lode_io.hpp"
#include "varint.hpp"
#include "panic.hpp"
#include "table_encode.hpp"

void print_usage(){
	printf("./choh_colour infile.png outfile.hoh speed\n\nspeed must be 1\nThis is an experimental colour version and it sucks.\n");
}

void colour_encode_entropy(uint8_t* in_bytes,size_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height*3);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode,range);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height*3;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + in_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}

	*(--outPointer) = 0b01000000;
}

int main(int argc, char *argv[]){
	if(argc < 4){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	size_t speed = atoi(argv[3]);

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);

	uint8_t* alpha_stripped = new uint8_t[width*height*3];
	for(size_t i=0;i<width*height;i++){
		alpha_stripped[i*3 + 0] = decoded[i*4 + 0];
		alpha_stripped[i*3 + 1] = decoded[i*4 + 1];
		alpha_stripped[i*3 + 2] = decoded[i*4 + 2];
	}
	delete[] decoded;

	printf("creating buffer\n");

	size_t max_elements = (width*height + 4096)*3;
	uint8_t* out_buf = new uint8_t[max_elements];
	uint8_t* out_end = out_buf + max_elements;
	uint8_t* outPointer = out_end;

	if(speed == 1){
		colour_encode_entropy(alpha_stripped, 256,width,height,outPointer);
	}
	else{
		panic("speed must be 1!\n");
	}
	delete[] alpha_stripped;

	printf("writing header\n");
	writeVarint_reverse((uint32_t)(height - 1),outPointer);
	writeVarint_reverse((uint32_t)(width - 1), outPointer);

	
	printf("file size %d\n",(int)(out_end - outPointer));


	write_file(argv[2],outPointer,out_end - outPointer);
	delete[] out_buf;

	return 0;
}
