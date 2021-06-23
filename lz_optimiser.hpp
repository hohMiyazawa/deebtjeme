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

	uint8_t max_back_x = inverse_prefix((width+1)/2)*2 + 1;

	size_t previous_match = 0;

	lz_triple firstTriple;
	lz_data[0] = firstTriple;
	lz_size = 1;

	for(int i=0;i<width*height;){
		size_t match_length = 0;
		size_t back_ref = 0;
		for(int step_back=1;step_back < limit && i - step_back + 1 > 0;step_back++){
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
			i++;
		}
		else{
			back_ref -= 1;
			match_length -= 1;
			lz_data[lz_size - 1].val_future = previous_match;
			uint8_t future_prefix = inverse_prefix(previous_match);
			uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
			lz_data[lz_size - 1].future_bits = prefix_extrabits(previous_match) + (future_extrabits << 24);
			lz_data[lz_size - 1].future = future_prefix;

			uint32_t back_x = back_ref % width;
			uint32_t back_y = back_ref / width;
			uint8_t back_x_prefix;
			uint8_t back_x_extrabits;
			uint32_t back_x_extrabits_value;
			if(width - back_x < back_x){
				back_x_prefix = max_back_x - inverse_prefix(width - back_x - 1);
				back_x_extrabits = extrabits_from_prefix(max_back_x - back_x_prefix);
				back_x_extrabits_value = prefix_extrabits(width - back_x - 1) + (back_x_extrabits << 24);
			}
			else{
				back_x_prefix = inverse_prefix(back_x);
				back_x_extrabits = extrabits_from_prefix(back_x_prefix);
				back_x_extrabits_value = prefix_extrabits(back_x) + (back_x_extrabits << 24);;
			}
			lz_data[lz_size].backref_x_bits = back_x_extrabits_value;
			lz_data[lz_size].backref_x = back_x_prefix;

			uint8_t back_y_prefix = inverse_prefix(back_y);
			uint8_t back_y_extrabits = extrabits_from_prefix(back_y_prefix);
			lz_data[lz_size].backref_y_bits = prefix_extrabits(back_y) + (back_y_extrabits << 24);
			lz_data[lz_size].backref_y = back_y_prefix;

			lz_data[lz_size].val_matchlen = match_length;
			uint8_t matchlen_prefix = inverse_prefix(match_length);
			uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
			lz_data[lz_size].matchlen_bits = prefix_extrabits(match_length) + (matchlen_extrabits << 24);
			lz_data[lz_size].matchlen = matchlen_prefix;

			lz_size++;
			previous_match = 0;
			i += match_length + 1;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;
	uint8_t future_prefix = inverse_prefix(previous_match);
	uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
	lz_data[lz_size - 1].future_bits = prefix_extrabits(previous_match) + (future_extrabits << 24);
	lz_data[lz_size - 1].future = future_prefix;

	//printf("first %d(%d) (incomplete)\n",(int)lz_data[0].future,(int)lz_data[0].val_future);
	//printf("%d %d %d(%d) %d\n",(int)lz_data[1].backref_x,(int)lz_data[1].backref_y,(int)lz_data[1].matchlen,(int)lz_data[1].val_matchlen,(int)lz_data[1].future);

	return lz_data;
}

#endif //LZ_OPTIMISER
