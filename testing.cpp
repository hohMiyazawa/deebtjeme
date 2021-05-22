#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <cmath>
#include <math.h>

#include "laplace.hpp"

void print_usage(){
	printf("\n");
}

int main(int argc, char *argv[]){
	if(argc < 2){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	uint8_t laplaceLevel = atoi(argv[1]);
	
	SymbolStats stats = laplace(laplaceLevel);
	for(size_t i=0;i<256;i++){
		printf("%d\n",(int)stats.freqs[i]);
	}

	return 0;
}
