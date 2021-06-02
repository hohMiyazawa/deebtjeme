#ifndef TABLE_DECODE_HEADER
#define TABLE_DECODE_HEADER

#include "bitreader.hpp"
#include "numerics.hpp"
#include "laplace.hpp"
#include "symbolstats.hpp"

SymbolStats decode_freqTable(BitReader& reader,size_t range,uint8_t& blocking){
	SymbolStats stats;
	blocking = reader.readBits(4);
	uint8_t mode = reader.readBits(4);
	size_t true_range = range;
	if(blocking){
		true_range = (range + (1 << (blocking - 1)) - 1) >> blocking;
	}
	if(mode == 0){
		for(size_t i=0;i<256;i++){
			if(i < range){
				stats.freqs[i] = 1;
			}
			else{
				stats.freqs[i] = 0;
			}
		}
		stats.normalize_freqs(1 << 16);
	}
	else if(mode <= 10){
		return laplace(mode,true_range);
	}
	else{
		size_t zero_pointer_length = log2_plus(range - 1);
		size_t zero_count_bits = log2_plus(range / zero_pointer_length);
		size_t zero_count = reader.readBits(zero_count_bits);
		for(size_t i=0;i<256;i++){
			stats.freqs[i] = 0;
		}
		if(zero_count == (1 << zero_count_bits) - 1){
			for(size_t i=0;i<true_range;i++){
				stats.freqs[i] = reader.readBits(1);
			}
		}
		else{
			size_t changes[zero_count + 1];
			for(size_t i=0;i<zero_count;i++){
				changes[i] = reader.readBits(zero_count_bits);
			}
			changes[zero_count = true_range];
			bool running = true;
			uint8_t index = 0;
			for(size_t i=0;i<true_range;i++){
				if(changes[index] == i){
					index++;
					running = !running;
				}
				stats.freqs[i] = running;
			}
		}
		if(mode == 11){
			//nothing more to read
		}
		else if(mode == 12){
			for(size_t i=0;i<true_range;i++){
				if(stats.freqs[i]){
					uint8_t magnitude = reader.readBits(2);
					uint8_t extrabits = (magnitude + 1) / 2 - 1;
					stats.freqs[i] = (1 << magnitude) + (reader.readBits(extrabits) << (magnitude - extrabits));
				}
			}
		}
		else if(mode == 13){
			for(size_t i=0;i<true_range;i++){
				if(stats.freqs[i]){
					uint8_t magnitude = reader.readBits(3);
					uint8_t extrabits = (magnitude + 1) / 2 - 1;
					stats.freqs[i] = (1 << magnitude) + (reader.readBits(extrabits) << (magnitude - extrabits));
				}
			}
		}
		else if(mode == 14){
			for(size_t i=0;i<true_range;i++){
				if(stats.freqs[i]){
					uint8_t magnitude = reader.readBits(4);
					uint8_t extrabits = (magnitude + 1) / 2 - 1;
					stats.freqs[i] = (1 << magnitude) + (reader.readBits(extrabits) << (magnitude - extrabits));
				}
			}
		}
		else if(mode == 15){
			for(size_t i=0;i<true_range;i++){
				if(stats.freqs[i]){
					uint8_t magnitude = reader.readBits(4);
					stats.freqs[i] = (1 << magnitude) + reader.readBits(magnitude);
				}
			}
		}
	}
	return stats;
}

#endif //TABLE_DECODE
