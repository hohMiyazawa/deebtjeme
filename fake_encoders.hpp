#ifndef FAKE_ENCODERS_HEADER
#define FAKE_ENCODERS_HEADER

#include "colour_simple_encoders.hpp"

void colour_optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
);

size_t colour_encode_combiner_dry(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height){
	size_t safety_margin = 3*width*height * (log2_plus(range - 1) + 1) + 2048;

	uint8_t alternates = 5;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing_end[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing_end[i] = miniBuffer[i] + safety_margin;
		trailing[i] = trailing_end[i];
	}
	colour_encode_entropy(in_bytes,range,width,height,trailing[0]);
	colour_encode_entropy_channel(in_bytes,range,width,height,trailing[1]);
	colour_encode_left(in_bytes,range,width,height,trailing[2]);
	colour_encode_ffv1(in_bytes,range,width,height,trailing[3]);
	colour_optimiser_entropyOnly(in_bytes,range,width,height,trailing[4],2);
/*
	colour_encode_entropy_quad(in_bytes,range,width,height,trailing[4]);*/
	for(size_t i=0;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		//printf("type %d: %d\n",(int)i,(int)diff);
	}

	uint8_t bestIndex = 0;
	size_t best = trailing_end[0] - trailing[0];
	for(size_t i=1;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		if(diff < best){
			best = diff;
			bestIndex = i;
		}
	}
	for(size_t i=0;i<alternates;i++){
		delete[] miniBuffer[i];
	}
	return best;
}

#endif //FAKE_ENCODERS
