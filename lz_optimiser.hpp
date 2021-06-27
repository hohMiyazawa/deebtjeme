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

	size_t limit = speed;

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

lz_triple* lz_dist_fast(
	uint8_t* in_bytes,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	size_t speed
){
	lz_triple* lz_data = new lz_triple[width*height];

	size_t limit = speed;

	uint8_t max_back_x = inverse_prefix((width+1)/2)*2 + 1;

	size_t previous_match = 0;

	lz_triple firstTriple;
	lz_data[0] = firstTriple;
	lz_size = 1;

	for(int i=0;i<width*height;){
		size_t future_iden = 0;
		for(size_t j=1;j + i < width*height;j++){
			if(
				in_bytes[i*3 + 0] == in_bytes[(i + j)*3 + 0]
				&& in_bytes[i*3 + 1] == in_bytes[(i + j)*3 + 1]
				&& in_bytes[i*3 + 2] == in_bytes[(i + j)*3 + 2]
			){
				future_iden++;
			}
			else{
				break;
			}
		}
		if(future_iden < 3){
		}
		/*else if(future_iden < 4){
			previous_match += future_iden + 2;
			i += future_iden + 2;
			continue;
		}*/
		else{
			size_t match_length = future_iden - 1;

			previous_match += 1;

			lz_data[lz_size - 1].val_future = previous_match ;
			uint8_t future_prefix = inverse_prefix(previous_match);
			uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
			lz_data[lz_size - 1].future_bits = prefix_extrabits(previous_match) + (future_extrabits << 24);
			lz_data[lz_size - 1].future = future_prefix;

			lz_data[lz_size].backref_x_bits = 0;
			lz_data[lz_size].backref_x = 0;

			lz_data[lz_size].backref_y_bits = 0;
			lz_data[lz_size].backref_y = 0;

			lz_data[lz_size].val_matchlen = match_length;
			uint8_t matchlen_prefix = inverse_prefix(match_length);
			uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
			lz_data[lz_size].matchlen_bits = prefix_extrabits(match_length) + (matchlen_extrabits << 24);
			lz_data[lz_size].matchlen = matchlen_prefix;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}

		size_t match_length = 0;
		size_t back_ref = 0;
		for(int step_back=2;step_back < limit && i - step_back + 1 > 0;step_back++){
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

lz_triple* lz_dist_fast_grey(
	uint8_t* in_bytes,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	size_t speed
){
	lz_triple* lz_data = new lz_triple[width*height];

	size_t limit = speed;

	uint8_t max_back_x = inverse_prefix((width+1)/2)*2 + 1;

	size_t previous_match = 0;

	lz_triple firstTriple;
	lz_data[0] = firstTriple;
	lz_size = 1;

	for(int i=0;i<width*height;){
		size_t future_iden = 0;
		for(size_t j=1;j + i < width*height;j++){
			if(
				in_bytes[i] == in_bytes[i + j]
			){
				future_iden++;
			}
			else{
				break;
			}
		}
		if(future_iden < 10){
		}
		else{
			size_t match_length = future_iden - 1;

			previous_match += 1;

			lz_data[lz_size - 1].val_future = previous_match ;
			uint8_t future_prefix = inverse_prefix(previous_match);
			uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
			lz_data[lz_size - 1].future_bits = prefix_extrabits(previous_match) + (future_extrabits << 24);
			lz_data[lz_size - 1].future = future_prefix;

			lz_data[lz_size].backref_x_bits = 0;
			lz_data[lz_size].backref_x = 0;

			lz_data[lz_size].backref_y_bits = 0;
			lz_data[lz_size].backref_y = 0;

			lz_data[lz_size].val_matchlen = match_length;
			uint8_t matchlen_prefix = inverse_prefix(match_length);
			uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
			lz_data[lz_size].matchlen_bits = prefix_extrabits(match_length) + (matchlen_extrabits << 24);
			lz_data[lz_size].matchlen = matchlen_prefix;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}

		size_t match_length = 0;
		size_t back_ref = 0;
		for(int step_back=2;step_back < limit && i - step_back + 1 > 0;step_back++){
			size_t len=0;
			for(;i + len < width*height;){
				if(
					in_bytes[i + len] == in_bytes[i - step_back + len]
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
					in_bytes[i + len] == in_bytes[i - yy*width + len]
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
		if(match_length < 10){
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

lz_triple* lz_dist_complete_grey(
	uint8_t* in_bytes,
	float* estimate,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	double value_tresh,
	size_t speed
){
	lz_triple* lz_data = new lz_triple[width*height];

	size_t limit = speed;

	uint8_t max_back_x = inverse_prefix((width+1)/2)*2 + 1;

	size_t previous_match = 0;

	lz_triple firstTriple;
	lz_data[0] = firstTriple;
	lz_size = 1;

	for(int i=0;i<width*height;){
		size_t future_iden = 0;
		double future_iden_value = 0;
		for(size_t j=1;j + i < width*height;j++){
			if(
				in_bytes[i] == in_bytes[i + j]
			){
				future_iden++;
				future_iden_value += estimate[i + j];
			}
			else{
				break;
			}
		}
		if(future_iden_value > value_tresh){
			size_t match_length = future_iden - 1;

			previous_match += 1;

			lz_data[lz_size - 1].val_future = previous_match ;
			uint8_t future_prefix = inverse_prefix(previous_match);
			uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
			lz_data[lz_size - 1].future_bits = prefix_extrabits(previous_match) + (future_extrabits << 24);
			lz_data[lz_size - 1].future = future_prefix;

			lz_data[lz_size].backref_x_bits = 0;
			lz_data[lz_size].backref_x = 0;

			lz_data[lz_size].backref_y_bits = 0;
			lz_data[lz_size].backref_y = 0;

			lz_data[lz_size].val_matchlen = match_length;
			uint8_t matchlen_prefix = inverse_prefix(match_length);
			uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
			lz_data[lz_size].matchlen_bits = prefix_extrabits(match_length) + (matchlen_extrabits << 24);
			lz_data[lz_size].matchlen = matchlen_prefix;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}
		else if(future_iden > 64){
			previous_match += future_iden + 1;
			i += future_iden + 1;
		}

		size_t match_length = 0;
		size_t back_ref = 0;
		double best_value = 0;
		for(int step_back=2;step_back < limit && i - step_back + 1 > 0;step_back++){
			size_t len=0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[i + len] == in_bytes[i - step_back + len]
				){
					value += estimate[i + len];
					len++;
				}
				else{
					break;
				}
			}
			if(value > best_value){
				match_length = len;
				back_ref = step_back;
				best_value = value;
				if(len > 64){
					break;
				}
			}
		}
		size_t y = i/width;
		for(size_t yy=0;yy < limit && y - (yy++);){
			size_t len=0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[i + len] == in_bytes[i - yy*width + len]
				){
					value += estimate[i + len];
					len++;
				}
				else{
					break;
				}
			}
			if(value > best_value){
				match_length = len;
				back_ref = yy*width;
				best_value = value;
			}
		}
		if(best_value < value_tresh){
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

lz_triple* lz_dist_grey(
	uint8_t* in_bytes,
	uint32_t width,
	uint32_t height,
	size_t& lz_size,
	size_t speed
){
	lz_triple* lz_data = new lz_triple[width*height];

	size_t limit = speed;

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
					in_bytes[i + len] == in_bytes[i - step_back + len]
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
					in_bytes[i + len] == in_bytes[i - yy*width + len]
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
		if(match_length < 6){
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
