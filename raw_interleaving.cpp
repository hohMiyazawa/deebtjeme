#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "rans_byte.h"
#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "filters.hpp"
#include "2dutils.hpp"

void print_usage(){
	printf("./decoder infile.hoh outfile.grey speed\n");
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

	uint32_t width  = atoi(argv[3]);
	uint32_t height = atoi(argv[4]);
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));

	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, width, height);

	uint8_t* out_bytes = new uint8_t[width*height];

	out_bytes[0] = filtered_bytes[0];
	for(size_t i=1;i<width;i++){
		out_bytes[i] = filtered_bytes[i] + out_bytes[i - 1];//top edge is always left-predicted
	}
	for(size_t y=1;y<height;y++){
		out_bytes[y * width] = filtered_bytes[y * width] + out_bytes[(y-1) * width];//left edge is always top-predicted
		for(size_t i=1;i<width;i++){
			uint8_t L = out_bytes[y * width + i - 1];
			uint8_t TL = out_bytes[(y-1) * width + i - 1];
			uint8_t T = out_bytes[(y-1) * width + i];
			if((i % 16) == 0 && (y % 16) == 0){
				out_bytes[(y * width) + i] = in_bytes[(y * width) + i];
				//out_bytes[(y * width) + i] = 128;
			}
			else if(((i % 4 == 3) && (y % 2)) || (y % 1)){
				out_bytes[(y * width) + i] = (
					median3(
						L,
						T,
						L + T - TL
					)
				);
			}
			else{
				out_bytes[(y * width) + i] = (
					filtered_bytes[y * width + i] + median3(
						L,
						T,
						L + T - TL
					)
				);
				int diff = ((int)out_bytes[(y * width) + i] - (int)L) + ((int)out_bytes[(y * width) + i] - (int)T);
				if(diff > 400 || diff < -400){
					out_bytes[(y * width) + i] = (
						median3(
							L,
							T,
							L + T - TL
						)
					);
				}
			}
		}
	}
	
	delete[] in_bytes;
	delete[] filtered_bytes;

	write_file(argv[2],out_bytes,width*height);

	delete[] out_bytes;
	return 0;
}
