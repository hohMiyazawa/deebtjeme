#ifndef TABLE_ENCODE_HEADER
#define TABLE_ENCODE_HEADER

#include "bitwriter.hpp"
#include "numerics.hpp"
#include "laplace.hpp"
#include "symbolstats.hpp"

SymbolStats encode_freqTable(SymbolStats freqs,BitWriter& sink, uint32_t range){

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
	if(sum >= (1 << 16)){
		freqs.normalize_freqs(1 << 16);
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
			//printf("zero count: %d %d\n",(int)zero_count,(int)changes[0]);
			for(size_t i=0;i<zero_count;i++){
				sink.writeBits(changes[i],zero_pointer_length);
			}
		}

		for(size_t i=0;i<range;i++){
			if(newFreqs.freqs[i]){
				if(newFreqs.freqs[i] == (1 << 16)){
					sink.writeBits(0,4);//doesn't matter what the magnitude is, gets scaled anyway
					break;
				}
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
	}
	else{
		for(size_t i=0;i<256;i++){
			uint8_t magnitude = log2_plus(freqs.freqs[i]) - 1;
			uint8_t extrabits = (magnitude + 1) / 2 - 1;
			uint16_t mask = 0;
			for(size_t m=0;m<extrabits;m++){
				mask = (mask << 1) + 1;
			}
			mask = (mask << (magnitude - extrabits));
			newFreqs.freqs[i] = (1 << magnitude) + mask & freqs.freqs[i];
			uint16_t residual = freqs.freqs[i] - newFreqs.freqs[i];
			newFreqs.freqs[i] += (residual << 1) & mask;
		}
		sink.writeBits(0,4);//no blocking
		sink.writeBits(14,4);//mode 14

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
				uint8_t extrabits_count = (magnitude + 1) / 2;
				if(extrabits_count){
					extrabits_count--;
				}
				uint16_t extraBits = (newFreqs.freqs[i] - (1 << magnitude)) >> (magnitude - extrabits_count);
				sink.writeBits(extraBits,extrabits_count);
			}
		}
	}
	newFreqs.normalize_freqs(1 << 16);
	return newFreqs;
}

SymbolStats encode_freqTable_dry(SymbolStats freqs,size_t& sink, uint32_t range){

	sink = 0;
	size_t sum = 0;
	for(size_t i=0;i<range;i++){
		sum += freqs.freqs[i];
	}
	SymbolStats newFreqs;
	if(sum == 0){
		sink += 8;
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
	sink += 8;

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

	if(zero_count == (1 << zero_count_bits) - 1){
		sink += zero_count_bits;
		sink += range;
	}
	else{
		sink += zero_count_bits;
		sink += zero_count * zero_pointer_length;
	}

	for(size_t i=0;i<range;i++){
		if(newFreqs.freqs[i]){
			uint8_t magnitude = log2_plus(newFreqs.freqs[i]) - 1;
			sink += 4;
			uint16_t extraBits = newFreqs.freqs[i] - (1 << magnitude);
			sink += magnitude;
		}
	}
	newFreqs.normalize_freqs(1 << 16);
	return newFreqs;
}

#endif //TABLE_ENCODE
