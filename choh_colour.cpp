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
	printf("./choh_colour infile.png outfile.hoh speed\n\nspeed must be 1 or 2\nThis is an experimental colour version and it sucks.\n");
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

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = 0b01000000;
}

void colour_encode_entropy_channel(uint8_t* in_bytes,size_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	SymbolStats stats_green;
	SymbolStats stats_red;
	SymbolStats stats_blue;
	for(size_t i=0;i<256;i++){
		stats_green.freqs[i] = 0;
		stats_red.freqs[i] = 0;
		stats_blue.freqs[i] = 0;
	}
	for(size_t i=0;i<width*height*3;i+=3){
		stats_green.freqs[in_bytes[i]]++;
		stats_red.freqs[in_bytes[i+1]]++;
		stats_blue.freqs[in_bytes[i+2]]++;
	}

	BitWriter tableEncode;
	SymbolStats table_green = encode_freqTable(stats_green,tableEncode,range);
	SymbolStats table_red =   encode_freqTable(stats_red  ,tableEncode,range);
	SymbolStats table_blue =  encode_freqTable(stats_blue ,tableEncode,range);
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
		RansEncPutSymbol(&rans, &outPointer, esyms_blue  + in_bytes[index*3 + 2]);
		RansEncPutSymbol(&rans, &outPointer, esyms_red   + in_bytes[index*3 + 1]);
		RansEncPutSymbol(&rans, &outPointer, esyms_green + in_bytes[index*3 + 0]);
	}
	RansEncFlush(&rans, &outPointer);

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	uint8_t* entropy_image = new uint8_t[3];
	entropy_image[0] = 0;
	entropy_image[1] = 1;
	entropy_image[2] = 2;
	colour_encode_entropy(
		entropy_image,
		3,
		1,
		1,
		outPointer
	);
	delete[] entropy_image;
	printf("---entropy image size: %d bytes\n",(int)(trailing - outPointer));
	writeVarint_reverse((uint32_t)(1 - 1),outPointer);
	writeVarint_reverse((uint32_t)(1 - 1), outPointer);

	*(--outPointer) = 3 - 1;
	*(--outPointer) = 0b01000010;
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
		alpha_stripped[i*3 + 0] = decoded[i*4 + 1];
		alpha_stripped[i*3 + 1] = decoded[i*4 + 0];
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
	else if(speed == 2){
		colour_encode_entropy_channel(alpha_stripped, 256,width,height,outPointer);
	}
	else{
		panic("speed must be 1 or 2!\n");
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
