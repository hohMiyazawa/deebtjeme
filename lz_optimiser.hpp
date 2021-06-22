#ifndef LZ_OPTIMISER_HEADER
#define LZ_OPTIMISER_HEADER

#include "prefix_coding.hpp"

lz_triple* lz_dist(
	uint8_t* in_bytes,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	size_t speed
){
	lz_triple* lz_data = new lz_triple[width*height];

	size_t limit = (64 << speed);

	size_t extrabits = 0;
	uint8_t max_back_x = inverse_prefix(width/2)*2 + 1;

	size_t previous_match = 0;

	lz_triple firstTriple;
	lz_data[0] = firstTriple;
	lz_size = 1;
	uint8_t bitbuffer_length = 0;
	uint32_t* partial_location;

	uint32_t cache_future = 0;
	uint32_t cache_back_x = 0;
	uint32_t cache_back_y = 0;
	uint32_t cache_matchlen = 0;

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
			back_ref -= 1;
			match_length -= 1;
			if(previous_match == cache_future){
				lz_data[lz_size - 1].future = 0;
				lz_data[lz_size - 1].future_bits = 0;
			}
			else{
				uint8_t future_prefix = inverse_prefix(previous_match);
				uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
				uint32_t future_extrabits_value = prefix_extrabits(previous_match);
				buffered_extrabits_writing(
					bitbuffer_length,
					future_extrabits_value,
					future_extrabits,
					partial_location,
					&lz_data[lz_size - 1].future_bits
				);
				lz_data[lz_size - 1].future = future_prefix + 1;
				cache_future = previous_match;
			}

			uint32_t back_x = back_ref % width;
			uint32_t back_y = back_ref / width;
			if(back_x == cache_back_x){
				lz_data[lz_size].backref_x = 0;
				lz_data[lz_size].backref_x_bits = 0;
			}
			else{
				uint8_t back_x_prefix;
				uint8_t back_x_extrabits;
				uint32_t back_x_extrabits_value;
				if(width - back_x < back_x){
					back_x_prefix = max_back_x - inverse_prefix(width - back_x - 1);
					back_x_extrabits = extrabits_from_prefix(max_back_x - back_x_prefix);
					back_x_extrabits_value = prefix_extrabits(width - back_x - 1);
				}
				else{
					back_x_prefix = inverse_prefix(back_x) + 1;
					back_x_extrabits = extrabits_from_prefix(back_x_prefix - 1);
					back_x_extrabits_value = prefix_extrabits(back_x);
				}
				buffered_extrabits_writing(
					bitbuffer_length,
					back_x_extrabits_value,
					back_x_extrabits,
					partial_location,
					&lz_data[lz_size].backref_x_bits
				);
				lz_data[lz_size].backref_x = back_x_prefix;
				cache_back_x = back_x;
			}
			if(back_y == cache_back_y){
				lz_data[lz_size].backref_y = 0;
				lz_data[lz_size].backref_y_bits = 0;
			}
			else{
				uint8_t back_y_prefix = inverse_prefix(back_y);
				uint8_t back_y_extrabits = extrabits_from_prefix(back_y_prefix);
				uint32_t back_y_extrabits_value = prefix_extrabits(back_y);
				buffered_extrabits_writing(
					bitbuffer_length,
					back_y_extrabits_value,
					back_y_extrabits,
					partial_location,
					&lz_data[lz_size].backref_y_bits
				);
				lz_data[lz_size].backref_y = back_y_prefix + 1;
				cache_back_y = back_y;
			}

			if(match_length == cache_matchlen){
				lz_data[lz_size].matchlen = 0;
				lz_data[lz_size].matchlen_bits = 0;
			}
			else{
				uint8_t matchlen_prefix = inverse_prefix(match_length);
				uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
				uint32_t matchlen_extrabits_value = prefix_extrabits(match_length);
				buffered_extrabits_writing(
					bitbuffer_length,
					matchlen_extrabits_value,
					matchlen_extrabits,
					partial_location,
					&lz_data[lz_size].matchlen_bits
				);
				lz_data[lz_size].matchlen = matchlen_prefix + 1;
				cache_matchlen = match_length;
			}

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	if(previous_match == cache_future){
		lz_data[lz_size - 1].future = 0;
	}
	else{
		uint8_t future_prefix = inverse_prefix(previous_match);
		uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
		uint32_t future_extrabits_value = prefix_extrabits(previous_match);
		buffered_extrabits_writing(
			bitbuffer_length,
			future_extrabits_value,
			future_extrabits,
			partial_location,
			&lz_data[lz_size - 1].future_bits
		);
		lz_data[lz_size - 1].future = future_prefix + 1;
		cache_future = previous_match;
	}

	return lz_data;
}

#endif //LZ_OPTIMISER
