#ifndef TABLE_ENCODE_HEADER
#define TABLE_ENCODE_HEADER

#include "bitwriter.hpp"
#include "numerics.hpp"
#include "laplace.hpp"
#include "symbolstats.hpp"

SymbolStats encode_freqTable(SymbolStats freqs,BitWriter& sink, uint32_t range){
	//current behaviour: always use accurate 4bit magnitude coding, even if less accurate tables are sometimes more compact

	//printf("first in buffer %d %d\n",(int)sink.buffer[0],(int)sink.length);


	size_t sum = 0;
	for(size_t i=0;i<range;i++){//technically, there should be no frequencies above the range
		sum += freqs.freqs[i];
	}
	SymbolStats newFreqs;
	if(sum == 0){//dumb things can happen
		//write a flat table, in case this gets used
		sink.writeBits(0,8);
		SymbolStats newFreqs;
		for(size_t i=0;i<256;i++){
			if(i < range){
				newFreqs.freqs[i] = 1;
			}
			else{
				newFreqs.freqs[i] = 0;
			}
		}
		newFreqs.normalize_freqs(1 << 16);
		return newFreqs;
	}
	if(sum > (1 << 16)){
		freqs.normalize_freqs(1 << 16);
	}
	for(size_t i=0;i<256;i++){
		newFreqs.freqs[i] = freqs.freqs[i];
	}
	sink.writeBits(0,4);//no blocking
	sink.writeBits(15,4);//mode 15

	size_t zero_pointer_length = log2_plus(range - 1);
	size_t zero_count_bits = log2_plus(range / zero_pointer_length - 1);
	size_t zero_count = 0;
	size_t changes[1 << zero_count_bits];
	bool running = true;
	for(size_t i=0;i<range;i++){
		if((bool)newFreqs.freqs[i] != running){
			running = (bool)newFreqs.freqs[i];
			changes[zero_count++] = i;
			if(zero_count == (1 << zero_count_bits) - 1){
				break;
			}
		}
	}
	//printf("  zero pointer info: %d,%d\n",(int)zero_pointer_length,(int)zero_count_bits);
	//printf("  zero count: %d\n",(int)zero_count);
	if(zero_count == (1 << zero_count_bits) - 1){
		sink.writeBits((1 << zero_count_bits) - 1,zero_count_bits);
		for(size_t i=0;i<range;i++){
			if(newFreqs.freqs[i]){
				sink.writeBits(1,1);
			}
			else{
				sink.writeBits(0,1);
			}
		}
	}
	else{
		sink.writeBits(zero_count,zero_count_bits);
		for(size_t i=0;i<zero_count;i++){
			sink.writeBits(changes[i],zero_pointer_length);
		}
	}

	for(size_t i=0;i<range;i++){
		if(newFreqs.freqs[i]){
			uint8_t magnitude = log2_plus(newFreqs.freqs[i]) - 1;
			sink.writeBits(magnitude,4);
			uint16_t extraBits = newFreqs.freqs[i] - (1 << magnitude);
			if(magnitude > 8){
				sink.writeBits(extraBits >> 8,magnitude - 8);
				sink.writeBits(extraBits % 256,8);
			}
			else{
				sink.writeBits(extraBits,magnitude);
			}
		}
	}
	newFreqs.normalize_freqs(1 << 16);
	return newFreqs;
}

#endif //TABLE_ENCODE
