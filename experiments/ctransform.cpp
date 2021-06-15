#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <cmath>
#include <math.h>

#include "../lode_io.hpp"
#include "../symbolstats.hpp"
#include "../colour_filter_utils.hpp"
#include "../entropy_estimation.hpp"

void print_usage(){
	printf("\n");
}

uint16_t delta(uint8_t transform,uint8_t green){
	return ((uint16_t)green) * ((uint16_t)transform + 1) >> 8;
}

uint8_t modulo(uint16_t val){
	while(val < 0){
		val += 256;
	}
	return val % 256;
}

int main(int argc, char *argv[]){
	if(argc < 5){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

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

	uint8_t r_elem = atoi(argv[2]);
	uint8_t b_elem = atoi(argv[3]);
	uint8_t tt_elem = atoi(argv[4]);

	int16_t rtrans[width*height];
	int16_t btrans[width*height];

	int rdiff[256];
	int bdiff[256];
	size_t diff_count[256];
	for(size_t i=0;i<256;i++){
		rdiff[i] = 0;
		bdiff[i] = 0;
		diff_count[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		rdiff[alpha_stripped[i*3 + 0]] += alpha_stripped[i*3 + 1] - alpha_stripped[i*3 + 0];
		bdiff[alpha_stripped[i*3 + 0]] += alpha_stripped[i*3 + 2] - alpha_stripped[i*3 + 0];
		diff_count[alpha_stripped[i*3 + 0]]++;
		rtrans[i] = alpha_stripped[i*3 + 1] - delta(r_elem,alpha_stripped[i*3 + 0]);
		btrans[i] = alpha_stripped[i*3 + 2] - delta(b_elem,alpha_stripped[i*3 + 0]) - delta(tt_elem,alpha_stripped[i*3 + 1]);
	}

/*
	for(size_t i=0;i<256;i++){
		if(diff_count[i] == 0){
			printf("%d 0 0 \n",(int)i);
		}
		else{
			printf("%d %f %f\n",(int)i,(double)rdiff[i]/diff_count[i],(double)bdiff[i]/diff_count[i]);
		}
	}
*/

	printf("test\n");

	SymbolStats rent;
	SymbolStats bent;
	for(size_t i=0;i<256;i++){
		rent.freqs[i] = 0;
		bent.freqs[i] = 0;
	}

	for(size_t i=0;i<width*height;i++){
		if(i < width){
			if(i == 0){
				rent.freqs[modulo(rtrans[i])]++;
				bent.freqs[modulo(btrans[i])]++;
			}
			else{
				uint16_t r_L = rtrans[i - 1];
				uint16_t b_L = btrans[i - 1];
				rent.freqs[modulo(rtrans[i] - r_L)]++;
				bent.freqs[modulo(btrans[i] - b_L)]++;
			}
		}
		else if(i % width == 0){
			uint16_t r_T = rtrans[i - width];
			uint16_t b_T = btrans[i - width];
			rent.freqs[modulo(rtrans[i] - r_T)]++;
			bent.freqs[modulo(btrans[i] - b_T)]++;
		}
		else{
			uint16_t r_L = rtrans[i - 1];
			uint16_t b_L = btrans[i - 1];
			uint16_t r_T = rtrans[i - width];
			uint16_t b_T = btrans[i - width];
			uint16_t r_TL= rtrans[i - width - 1];
			uint16_t b_TL = btrans[i - width - 1];
			rent.freqs[modulo(rtrans[i] - ffv1(r_L,r_T,r_TL))]++;
			bent.freqs[modulo(btrans[i] - ffv1(b_L,b_T,b_TL))]++;
		}
	}

	double rchannel_ent = estimateEntropy_freq(rent, width*height)/8;
	double bchannel_ent = estimateEntropy_freq(bent, width*height)/8;
	printf("rchan: %f\n",rchannel_ent);
	printf("bchan: %f\n",bchannel_ent);

	delete[] alpha_stripped;
	return 0;
}
