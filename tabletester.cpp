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
	SymbolStats stats2;
	stats.count_freqs(filtered_bytes, in_size);
	for(size_t i=0;i<256;i++){
		stats2.freqs[i] = stats.freqs[i];
	}

	stats2.normalize_freqs(1 << 16);
	size_t total2 = 0;
	for(size_t i=0;i<256;i++){
		total2 += stats2.freqs[i];
	}

	double sum = 0;
	double sum2 = 0;
	printf("---\n");
	for(size_t i=0;i<256;i++){
		printf("%d\n",(int)stats.freqs[i]);
		if(stats.freqs[i]){
			sum -= stats.freqs[i]*std::log2((double)stats.freqs[i]/in_size);
		}
		if(stats.freqs[i]){
			sum2 -= stats.freqs[i]*std::log2((double)stats2.freqs[i]/total2);
		}
	}
	printf("---\n");
	printf("entropy : %f\n",sum);
	printf("entropy2: %f\n",sum2);
	SymbolStats stats3;
	for(size_t i=0;i<256;i++){
		if(stats2.freqs[i] == 0){
			stats3.freqs[i] = 0;
		}
		else if(stats2.freqs[i] == 1){
			stats3.freqs[i] = 1;
		}
		else if(stats2.freqs[i] == 2){
			stats3.freqs[i] = 2;
		}
		else if(stats2.freqs[i] < 6){
			stats3.freqs[i] = 4;
		}
		else if(stats2.freqs[i] < 12){
			stats3.freqs[i] = 8;
		}
		else if(stats2.freqs[i] < 24){
			stats3.freqs[i] = 16;
		}
		else if(stats2.freqs[i] < 48){
			stats3.freqs[i] = 32;
		}
		else if(stats2.freqs[i] < 96){
			stats3.freqs[i] = 64;
		}
		else if(stats2.freqs[i] < 192){
			stats3.freqs[i] = 128;
		}
		else if(stats2.freqs[i] < 384){
			stats3.freqs[i] = 256;
		}
		else if(stats2.freqs[i] < 768){
			stats3.freqs[i] = 512;
		}
		else if(stats2.freqs[i] < 1536){
			stats3.freqs[i] = 1024;
		}
		else if(stats2.freqs[i] < 3072){
			stats3.freqs[i] = 2048;
		}
		else if(stats2.freqs[i] < 4096 + 2048){
			stats3.freqs[i] = 4096;
		}
		else if(stats2.freqs[i] < 8192 + 4096){
			stats3.freqs[i] = 8192;
		}
		else if(stats2.freqs[i] < 16384 + 8192){
			stats3.freqs[i] = 16384;
		}
		else{
			stats3.freqs[i] = 32768;
		}
	}
	size_t total3 = 0;
	for(size_t i=0;i<256;i++){
		total3 += stats3.freqs[i];
	}
	double sum3 = 0;
	for(size_t i=0;i<256;i++){
		if(stats.freqs[i]){
			sum3 -= stats.freqs[i]*std::log2((double)stats3.freqs[i]/total3);
		}
	}
	int cost3 = 0;
	for(size_t i=0;i<256;i++){
		if(stats3.freqs[i]){
			cost3 += 4;
		}
		else{
			cost3 += 10;
			size_t j=1;
			for(;(j<64) && (i + j) < 256;j++){
				if(stats3.freqs[i + j] != 0){
					break;
				}
			}
			i += (j - 1);
		}
	}
	printf("entropy3: %f %d\n",sum3,cost3);


	

	delete[] filtered_bytes;
	return 0;
}
