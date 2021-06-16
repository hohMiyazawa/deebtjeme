#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <cmath>
#include <math.h>

#include "file_io.hpp"
#include "2dutils.hpp"
#include "lode_io.hpp"
#include "varint.hpp"
#include "simple_encoders.hpp"
#include "colour_simple_encoders.hpp"

void print_usage(){
	printf("./choh infile.png outfile.hoh speed\n\nspeed is a number from 0-8\n");
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
	bool greyscale = greyscale_test(decoded,width,height);
	if(greyscale){
		printf("converting to greyscale\n");
		uint8_t* grey = new uint8_t[width*height];
		for(size_t i=0;i<width*height;i++){
			grey[i] = decoded[i*4 + 1];
		}
		delete[] decoded;

		size_t max_elements = width*height + 4096;
		uint8_t* out_buf = new uint8_t[max_elements];
		uint8_t* out_end = out_buf + max_elements;
		uint8_t* outPointer = out_end;

		//calls here

		delete[] grey;

		writeVarint_reverse((uint32_t)(height - 1),outPointer);
		writeVarint_reverse((uint32_t)(width - 1), outPointer);

		
		printf("file size %d\n",(int)(out_end - outPointer));


		write_file(argv[2],outPointer,out_end - outPointer);
		delete[] out_buf;
	}
	else{
		uint8_t* alpha_stripped = new uint8_t[width*height*3];
		for(size_t i=0;i<width*height;i++){
			alpha_stripped[i*3 + 0] = decoded[i*4 + 1];
			alpha_stripped[i*3 + 1] = decoded[i*4 + 0];
			alpha_stripped[i*3 + 2] = decoded[i*4 + 2];
		}
		delete[] decoded;

		size_t max_elements = (width*height + 4096)*3;
		uint8_t* out_buf = new uint8_t[max_elements];
		uint8_t* out_end = out_buf + max_elements;
		uint8_t* outPointer = out_end;

		if(speed == 0){
			colour_encode_raw(alpha_stripped, 256, width, height, outPointer);
		}
		else if(speed == 1){
			colour_encode_entropy(alpha_stripped, 256, width, height, outPointer);
		}
		else if(speed == 2){
			colour_encode_entropy_channel(alpha_stripped, 256, width, height, outPointer);
		}
		else if(speed == 3){
			colour_encode_ffv1(alpha_stripped, 256, width, height, outPointer);
		}
		else if(speed == 4){
			colour_encode_ffv1_subGreen(alpha_stripped, 256, width, height, outPointer);
		}

		delete[] alpha_stripped;

		writeVarint_reverse((uint32_t)(height - 1),outPointer);
		writeVarint_reverse((uint32_t)(width - 1), outPointer);

		
		printf("file size %d\n",(int)(out_end - outPointer));


		write_file(argv[2],outPointer,out_end - outPointer);
		delete[] out_buf;
	}

	return 0;
}
