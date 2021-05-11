#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <cmath>
#include <math.h>

#include "file_io.hpp"
#include "symbolstats.hpp"
#include "filter_utils.hpp"
#include "filters.hpp"
#include "2dutils.hpp"

void print_usage(){
	printf("./tabletester infile.grey width height predictor\n\nstats of residuals\n");
}

int main(int argc, char *argv[]){
	if(argc < 5){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	uint32_t width = atoi(argv[2]);
	uint32_t height = atoi(argv[3]);
	uint8_t predictor = atoi(argv[4]);

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
	printf("width : %d\n",(int)(width));
	printf("height: %d\n",(int)(height));
	printf("predictor: %d\n",(int)predictor);

	uint8_t* filtered_bytes = filter_all(in_bytes, width, height, predictor);

	delete[] in_bytes;

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, in_size);

	double sum = 0;
	printf("---\n");
	for(size_t i=0;i<256;i++){
		printf("%d\n",(int)stats.freqs[i]);
		if(stats.freqs[i]){
			sum -= stats.freqs[i]*std::log2((double)stats.freqs[i]/in_size);
		}
	}
	printf("---\n");
	printf("entropy: %f\n",sum);

	

	delete[] filtered_bytes;
	return 0;
}
