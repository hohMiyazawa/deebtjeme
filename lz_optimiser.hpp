#ifndef LZ_OPTIMISER_HEADER
#define LZ_OPTIMISER_HEADER

#include "prefix_coding.hpp"

uint32_t* lz_dist(
	uint8_t* in_bytes,
	float* costmap,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	size_t speed
){
	uint32_t* lz_data = new uint32_t[width*height + 64];
	lz_size = 0;

	size_t limit = (64 << speed);

	size_t previous_match = 0;

	for(int i=0;i<width*height;){
		double saved = 0;
		size_t match_length = 0;
		size_t back_ref = 0;
		size_t cancelled = 0;
/*
		if(i % width == 0){
			printf("row %d\n",(int)(i/width));
		}
*/
		for(int step_back=1;step_back < limit && i - step_back > 0;step_back++){
			double cur_saved = 0;
			size_t len=0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - step_back + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - step_back + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - step_back + len)*3 + 2]
				){
					cur_saved += costmap[(i + len)];
					len++;
				}
				else{
					break;
				}
			}
			double symbolCost = 24;
			if(len > 128){
				symbolCost += 8;
			}
			if(step_back > 128){
				symbolCost += 8;
			}
			cur_saved -= symbolCost;
			if(cur_saved > saved){
				saved = cur_saved;
				match_length = len;
				back_ref = step_back;
				if(len > 64){
					break;
				}
			}
			else if(len > 64){
				//printf("test %d\n",(int)len);
				cancelled = len;
				break;
			}
		}
		size_t y = i/width;
		if(y && !cancelled){
		for(size_t yy=0;yy < limit && y - (yy++);){
			double cur_saved = 0;
			size_t len=0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - yy*width + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - yy*width + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - yy*width + len)*3 + 2]
				){
					cur_saved += costmap[(i + len)];
					len++;
				}
				else{
					break;
				}
			}
			double symbolCost = 24;
			if(len > 128){
				symbolCost += 8;
			}
			if(yy*width > 128){
				symbolCost += 8;
			}
			cur_saved -= symbolCost;
			if(cur_saved > saved){
				saved = cur_saved;
				match_length = len;
				back_ref = yy*width;
			}
		}
		}
		if(saved == 0){
			i += 1 + cancelled;
			previous_match += 1 + cancelled;
		}
		else{
			lz_data[lz_size++] = previous_match;
			previous_match = 0;
			lz_data[lz_size++] = back_ref - 1;
			lz_data[lz_size++] = match_length - 1;
			i += match_length;
		}
	}
	lz_data[lz_size++] = previous_match;
/*
	SymbolStats rr_1;
	SymbolStats rr_2;
	SymbolStats rr_3;
	for(size_t i=0;i<256;i++){
		rr_1.freqs[i] = 0;
		rr_2.freqs[i] = 0;
		rr_3.freqs[i] = 0;
	}
	double extra = 0;
	for(size_t i=1;i<lz_size;i+=3){
		uint8_t prefix1 = val_to_prefix(lz_data[i] + 1);
		rr_1.freqs[prefix1]++;
		uint8_t prefix2 = val_to_prefix(lz_data[i+1] + 1);
		rr_2.freqs[prefix2]++;
		uint8_t prefix3 = val_to_prefix(lz_data[i+2] + 1);
		rr_3.freqs[prefix3]++;
		extra += prefix_to_extra(prefix1);
		extra += prefix_to_extra(prefix2);
		extra += prefix_to_extra(prefix3);
	}
	double rr_1_ent = estimateEntropy_freq(rr_1, (lz_size - 1)/3);
	double rr_2_ent = estimateEntropy_freq(rr_2, (lz_size - 1)/3);
	double rr_3_ent = estimateEntropy_freq(rr_3, (lz_size - 1)/3);
	printf("rr %f %f %f %f = %f\n",rr_1_ent,rr_2_ent,rr_3_ent,extra,(rr_1_ent+rr_2_ent+rr_3_ent+extra)/8);
*/

	return lz_data;
}

#endif //LZ_OPTIMISER
