#ifndef LZ_OPTIMISER_HEADER
#define LZ_OPTIMISER_HEADER

#include "prefix_coding.hpp"

uint16_t* lz_dist2(
	uint8_t* in_bytes,
	uint32_t width,
	uint32_t height,
	size_t& lz_pro_size,
	size_t speed
){
	uint32_t lz_data[width*height + 64];
	size_t lz_size = 0;

	size_t limit = (64 << speed);

	size_t previous_match = 0;

	for(int i=0;i<width*height;){
		size_t match_length = 0;
		size_t back_ref = 0;
		for(int step_back=1;step_back < limit && i - step_back > 0;step_back++){
			size_t len=0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - step_back + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - step_back + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - step_back + len)*3 + 2]
				){
					len++;
				}
				else{
					break;
				}
			}
			if(len > match_length){
				match_length = len;
				back_ref = step_back;
				if(len > 64){
					break;
				}
			}
		}
		size_t y = i/width;
		for(size_t yy=0;yy < limit && y - (yy++);){
			size_t len=0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - yy*width + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - yy*width + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - yy*width + len)*3 + 2]
				){
					len++;
				}
				else{
					break;
				}
			}
			if(len > match_length){
				match_length = len;
				back_ref = yy*width;
			}
		}
		if(match_length < 3){
			previous_match += 1;
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

	lz_pro_size = 0;
	uint16_t* lz_data_processed = new uint16_t[width*height + 64];

	size_t extrabits = 0;

	uint8_t max_back_x = inverse_prefix(width/2)*2 + 1;

	for(size_t i=0;i<(lz_size - 1);i+=3){
		uint8_t future_prefix = inverse_prefix(lz_data[i]);
		extrabits += extrabits_from_prefix(future_prefix);
		lz_data_processed[lz_pro_size++] = future_prefix;
		uint32_t back_x = lz_data[i+1] % width;
		uint32_t back_y = lz_data[i+1] / width;
		if(width - back_x < back_x){
			uint8_t back_x_prefix = inverse_prefix(back_x);
			extrabits += extrabits_from_prefix(back_x_prefix);
			lz_data_processed[lz_pro_size++] = max_back_x - back_x_prefix;
		}
		else{
			uint8_t back_x_prefix = inverse_prefix(back_x);
			extrabits += extrabits_from_prefix(back_x_prefix);
			lz_data_processed[lz_pro_size++] = back_x_prefix;
		}
		uint8_t back_y_prefix = inverse_prefix(back_y);
		extrabits += extrabits_from_prefix(back_y_prefix);
		lz_data_processed[lz_pro_size++] = back_y_prefix;
		uint32_t matchlen_prefix = inverse_prefix(lz_data[i+2]);
		extrabits += extrabits_from_prefix(matchlen_prefix);
		lz_data_processed[lz_pro_size++] = matchlen_prefix;
	}
	uint32_t matchlen_prefix = inverse_prefix(lz_data[lz_size]);
	extrabits += extrabits_from_prefix(matchlen_prefix);
	lz_data_processed[lz_pro_size++] = matchlen_prefix;

	return lz_data_processed;
}

#endif //LZ_OPTIMISER
