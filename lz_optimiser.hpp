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

	lz_data[0].val_future = 0;
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
			if(len > match_length && yy*width < (1 << 20)){
				match_length = len;
				back_ref = yy*width;
			}
		}
		if(match_length < 3){
			previous_match += 1;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;

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

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
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
			lz_data[lz_size].val_backref = 0;
			lz_data[lz_size].val_matchlen = match_length;

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
			if(len > match_length && yy*width < (1 << 20)){
				match_length = len;
				back_ref = yy*width;
			}
		}
		if(match_length < 3){
			previous_match += 1;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;

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

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
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

			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = 0;
			lz_data[lz_size].val_matchlen = match_length;

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
			if(len > match_length && yy*width < (1 << 20)){
				match_length = len;
				back_ref = yy*width;
			}
		}
		if(match_length < 10){
			previous_match++;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;
	return lz_data;
}

lz_triple* lz_dist_complete(
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

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
	lz_size = 1;

	for(int i=0;i<width*height;){
		//printf("i %d\n",(int)i);

		size_t future_iden = 0;
		double future_iden_value = 0;
		for(size_t j=1;j + i < width*height;j++){
			if(
				in_bytes[i*3 + 0] == in_bytes[(i + j)*3 + 0]
				&& in_bytes[i*3 + 1] == in_bytes[(i + j)*3 + 1]
				&& in_bytes[i*3 + 2] == in_bytes[(i + j)*3 + 2]
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

			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = 0;
			lz_data[lz_size].val_matchlen = match_length;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}
		else if(future_iden > 64){
			previous_match += future_iden + 1;
			i += future_iden + 1;
			continue;
		}

		size_t match_length = 0;
		size_t back_ref = 0;
		double best_value = 0;
		for(int step_back=2;step_back < limit && i - step_back + 1 > 0;step_back++){
			size_t len = 0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - step_back + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - step_back + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - step_back + len)*3 + 2]
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
				//printf("%d i+len\n",(int)(i + len));
				back_ref = step_back;
				best_value = value;
				if(len > 64){
					break;
				}
			}
		}
		size_t y = i/width;
		for(size_t yy=0;yy < limit && y - (yy++);){
			size_t len = 0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - yy*width + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - yy*width + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - yy*width + len)*3 + 2]
				){
					value += estimate[i + len];
					len++;
				}
				else{
					break;
				}
			}
			if(value > best_value && yy*width < (1 << 20)){
				match_length = len;
				//printf("%d i2+len\n",(int)(i + len));
				back_ref = yy*width;
				best_value = value;
			}
		}
		if(best_value < value_tresh){
			previous_match++;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;

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

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
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

			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = 0;
			lz_data[lz_size].val_matchlen = match_length;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}
		else if(future_iden > 64){
			previous_match += future_iden + 1;
			i += future_iden + 1;
			continue;
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
			if(value > best_value && yy*width < (1 << 20)){
				match_length = len;
				back_ref = yy*width;
				best_value = value;
			}
		}
		if(best_value < value_tresh){
			previous_match++;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;
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

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
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
			if(len > match_length && yy*width < (1 << 20)){
				match_length = len;
				back_ref = yy*width;
			}
		}
		if(match_length < 6){
			previous_match++;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;


			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;

	return lz_data;
}

lz_triple_c* lz_translator(
	lz_triple* lz_data,
	uint32_t width,
	size_t& lz_size
){
	lz_triple_c* reval = new lz_triple_c[lz_size];
	for(size_t i=0;i<lz_size;i++){
		reval[i].backref = 0;
		reval[i].matchlen = 0;
		reval[i].future = 0;
		reval[i].backref_bits = 0;
		reval[i].matchlen_bits = 0;
		reval[i].future_bits = 0;
	}
	for(size_t i=1;i<lz_size;i++){
		size_t back_x = lz_data[i].val_backref % width;
		size_t back_y = lz_data[i].val_backref / width;
		reval[i].backref_bits = 0;

		if(back_x < 8 && back_y < 8){
			reval[i].backref = reverse_lut[(7 - back_y) * 16 + 7 - back_x];
			//printf("what %d\n",(int)((7 - back_y) * 16 + 7 - back_x));
			//printf("lut %d\n",(int)reverse_lut[(7 - back_y) * 16 + 7 - back_x]);
		}
		else if(width - back_x <= 8 && back_y < 7){
			reval[i].backref = reverse_lut[(6 - back_y) * 16 + 7 + width - back_x];
			//printf("what %d\n",(int)((6 - back_y) * 16 + 7 + width - back_x));
			//printf("lut %d\n",(int)reverse_lut[(6 - back_y) * 16 + 7 + width - back_x]);
		}
		else if(width - back_x == 1 && back_y < 1024){
			uint8_t vertical_prefix = inverse_prefix(back_y);
			//printf("back_y %d\n",(int)vertical_prefix);
			reval[i].backref = 140 + vertical_prefix;

			uint8_t extrabits = extrabits_from_prefix(vertical_prefix);
			reval[i].backref_bits = prefix_extrabits(back_y) + (extrabits << 24);
		}
		else if(back_x < 16 && back_y > 0 && back_y <= 1024){
			uint8_t vertical_prefix = inverse_prefix(back_y - 1);
			reval[i].backref = 120 + vertical_prefix;

			uint8_t extrabits = extrabits_from_prefix(vertical_prefix);
			//printf("xoff: %d %d %d | %d x %d | %d\n",(int)lz_data[i].val_backref,(int)(15 - back_x),(int)vertical_prefix,(int)back_x,(int)back_y,(int));
			reval[i].backref_bits = ((prefix_extrabits(back_y - 1) << 5) + (15 - back_x)) + ((extrabits + 5) << 24);
		}
		else if(width - back_x <= 16 && back_y < 1024){
			uint8_t vertical_prefix = inverse_prefix(back_y);
			reval[i].backref = 120 + vertical_prefix;

			uint8_t extrabits = extrabits_from_prefix(vertical_prefix);
			//printf("xoff: %d %d %d\n",(int)lz_data[i].val_backref,(int)(15 + width - back_x),(int)vertical_prefix);
			reval[i].backref_bits = ((prefix_extrabits(back_y) << 5) + (15 + width - back_x)) + ((extrabits + 5) << 24);
		}
		else{
			uint8_t scanline_prefix = inverse_prefix(lz_data[i].val_backref);
			reval[i].backref = 160 + scanline_prefix;

			uint8_t extrabits = extrabits_from_prefix(scanline_prefix);
			reval[i].backref_bits = prefix_extrabits(lz_data[i].val_backref) + (extrabits << 24);
		}

		uint8_t matchlen_prefix = inverse_prefix(lz_data[i].val_matchlen);
		reval[i].matchlen = matchlen_prefix;

		uint8_t matchlen_extrabits = extrabits_from_prefix(matchlen_prefix);
		reval[i].matchlen_bits = prefix_extrabits(lz_data[i].val_matchlen) + (matchlen_extrabits << 24);

		uint8_t future_prefix = inverse_prefix(lz_data[i].val_future);
		reval[i].future = future_prefix;
		uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
		reval[i].future_bits = prefix_extrabits(lz_data[i].val_future) + (future_extrabits << 24);

	}
	uint8_t future_prefix = inverse_prefix(lz_data[0].val_future);
	reval[0].future = future_prefix;
	uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
	reval[0].future_bits = prefix_extrabits(lz_data[0].val_future) + (future_extrabits << 24);

	return reval;
}

void lz_cost(
	lz_triple_c* lz_symbols,
	size_t& lz_size,
	double*& backref_cost,
	double*& matchlen_cost,
	double*& future_cost
){
	SymbolStats stats_backref;
	SymbolStats stats_matchlen;
	SymbolStats stats_future;
	for(size_t i=0;i<256;i++){
		stats_backref.freqs[i] = 0;
		stats_matchlen.freqs[i] = 0;
		stats_future.freqs[i] = 0;
	}
	for(size_t i=1;i<lz_size;i++){
		stats_backref.freqs[lz_symbols[i].backref]++;
		stats_matchlen.freqs[lz_symbols[i].matchlen]++;
		stats_future.freqs[lz_symbols[i].future]++;
	}
	stats_future.freqs[lz_symbols[0].future]++;

	backref_cost = entropyLookup(stats_backref);
	matchlen_cost = entropyLookup(stats_matchlen);
	future_cost = entropyLookup(stats_future);

	for(size_t i=0;i<20;i++){
		uint8_t extrabits = extrabits_from_prefix(i);
		backref_cost[120 + i] += extrabits;
		backref_cost[140 + i] += extrabits + 5;
	}
	for(size_t i=0;i<40;i++){
		uint8_t extrabits = extrabits_from_prefix(i);
		backref_cost[160 + i] += extrabits;
		matchlen_cost[i] += extrabits;
		future_cost[i] += extrabits;
	}
}

lz_triple_c* lz_pruner(
	float* estimate,
	uint32_t width,
	lz_triple* lz_data,
	size_t& lz_size
){

	lz_triple_c* reval = lz_translator(
		lz_data,
		width,
		lz_size
	);

	double* backref_cost;
	double* matchlen_cost;
	double* future_cost;

	lz_cost(
		reval,
		lz_size,
		backref_cost,
		matchlen_cost,
		future_cost
	);
/*
	TODO: some context swapping optimisations here?
*/
/*
	bool singles_blocked = false;
	bool blocked[120];
	for(size_t i=0;i<120;i++){
		blocked[i] = false;
	}
	for(size_t y=0;y<7;y++){
		for(size_t x=0;x<15;x++){
			uint8_t location = y*16 + x;
			uint8_t prefix = reverse_lut[location];
			if(stats_backref.freqs[prefix] && backref_cost[prefix] > backref_cost[140 + inverse_prefix(6 - y)]){
				printf("   yay %d!\n",stats_backref.freqs[prefix]);
				blocked[prefix] = true;
				singles_blocked = true;
				stats_backref.freqs[140 + inverse_prefix(6 - y)] += stats_backref.freqs[prefix];
				stats_backref.freqs[prefix] = 0;
			}
		}
	}
	if(singles_blocked){
		for(size_t i=1;i<lz_size;i++){
			if(reval[i].backref < 120 && blocked[reval[i].backref] == true){
				printf("mingw\n");
			}
		}
		delete[] backref_cost;
		backref_cost = entropyLookup(stats_backref);
	}
*/

//end context swapping


	size_t index = lz_data[0].val_future;
	double total_minus = 0;

	size_t newIndex = 1;


	for(size_t i=1;i<lz_size;i++){
		double saved = 0;
		for(size_t j=0;j<=lz_data[i].val_matchlen;j++){
			saved += estimate[index + j];
		}
		saved -= backref_cost[reval[i].backref];
		saved -= matchlen_cost[reval[i].matchlen];
		saved -= (
				future_cost[reval[i].future]
				+ future_cost[reval[i-1].future]
			) - future_cost[
				inverse_prefix(
					lz_data[i].val_future
					+ lz_data[i-1].val_future
					+ lz_data[i].val_matchlen
					+ 1
				)
			];
		if(saved < 0){
			total_minus += saved;
			lz_data[newIndex - 1].val_future += lz_data[i].val_future + lz_data[i].val_matchlen + 1;
			uint8_t future_prefix = inverse_prefix(lz_data[newIndex - 1].val_future);
			reval[newIndex - 1].future = future_prefix;

			uint8_t future_extrabits = extrabits_from_prefix(future_prefix);
			reval[newIndex - 1].future_bits = prefix_extrabits(lz_data[newIndex - 1].val_future) + (future_extrabits << 24);
		}
		else{
			lz_data[newIndex] = lz_data[i];
			reval[newIndex] = reval[i];
			newIndex++;
		}
		
		index += lz_data[i].val_matchlen + 1 + lz_data[i].val_future;
	}
	printf("pruned %d matches, saving %f bits\n",(int)(lz_size - newIndex),-total_minus);
	lz_size = newIndex;

	delete[] backref_cost;
	delete[] matchlen_cost;
	delete[] future_cost;

	return reval;
}

lz_triple* lz_dist_selfAware(
	uint8_t* in_bytes,
	float* estimate,
	uint32_t width,
	uint32_t height,
	lz_triple* lz_data,
	lz_triple_c* lz_symbols,
	size_t& lz_size,
	double value_tresh,
	size_t speed
){
	double* backref_cost;
	double* matchlen_cost;
	double* future_cost;

	lz_cost(
		lz_symbols,
		lz_size,
		backref_cost,
		matchlen_cost,
		future_cost
	);

	size_t limit = speed;

	size_t previous_match = 0;

	lz_data[0].val_future = 0;
	lz_size = 1;

	for(int i=0;i<width*height;){
		//printf("i %d\n",(int)i);

		size_t future_iden = 0;
		double future_iden_value = 0;
		for(size_t j=1;j + i < width*height;j++){
			if(
				in_bytes[i*3 + 0] == in_bytes[(i + j)*3 + 0]
				&& in_bytes[i*3 + 1] == in_bytes[(i + j)*3 + 1]
				&& in_bytes[i*3 + 2] == in_bytes[(i + j)*3 + 2]
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

			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = 0;
			lz_data[lz_size].val_matchlen = match_length;

			lz_size++;
			previous_match = 0;
			i += match_length + 2;
			continue;
		}
		else if(future_iden > 64){
			previous_match += future_iden + 1;
			i += future_iden + 1;
			continue;
		}

		size_t match_length = 0;
		size_t back_ref = 0;
		double best_value = 0;
		for(int step_back=2;step_back < limit && i - step_back + 1 > 0;step_back++){
			size_t len = 0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - step_back + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - step_back + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - step_back + len)*3 + 2]
				){
					value += estimate[i + len];
					len++;
				}
				else{
					break;
				}
			}

			size_t back_x = (step_back - 1) % width;
			size_t back_y = (step_back - 1) / width;

			if(back_x < 8 && back_y < 8){
				value -= backref_cost[reverse_lut[(7 - back_y) * 16 + 7 - back_x]];
			}
			else if(width - back_x <= 8 && back_y < 7){
				value -= backref_cost[reverse_lut[(6 - back_y) * 16 + 7 + width - back_x]];
			}
			else if(back_x < 16 && back_y > 0 && back_y <= 1024){
				uint8_t vertical_prefix = inverse_prefix(back_y - 1);
				value -= backref_cost[120 + vertical_prefix];
			}
			else if(width - back_x <= 16 && back_y < 1024){
				uint8_t vertical_prefix = inverse_prefix(back_y);
				value -= backref_cost[120 + vertical_prefix];
			}
			else{
				value -= backref_cost[160 + inverse_prefix(step_back - 1)];
			}

			//value -= backref_cost[160 + inverse_prefix(step_back - 1)];
			value -= matchlen_cost[inverse_prefix(len - 1)];
			value -= future_cost[inverse_prefix(previous_match)];

			if(value > best_value){
				match_length = len;
				//printf("%d i+len\n",(int)(i + len));
				back_ref = step_back;
				best_value = value;
				if(len > 64){
					break;
				}
			}
		}
		size_t y = i/width;
		for(size_t yy=0;yy < limit && y - (yy++);){
			size_t len = 0;
			double value = 0;
			for(;i + len < width*height;){
				if(
					in_bytes[(i + len)*3 + 0] == in_bytes[(i - yy*width + len)*3 + 0]
					&& in_bytes[(i + len)*3 + 1] == in_bytes[(i - yy*width + len)*3 + 1]
					&& in_bytes[(i + len)*3 + 2] == in_bytes[(i - yy*width + len)*3 + 2]
				){
					value += estimate[i + len];
					len++;
				}
				else{
					break;
				}
			}

			if(yy*width < (1 << 20)){
				size_t back_y = (yy*width - 1) / width;

				uint8_t vertical_prefix = inverse_prefix(back_y);
				value -= backref_cost[140 + vertical_prefix];

				//value -= backref_cost[160 + inverse_prefix(yy*width - 1)];
				value -= matchlen_cost[inverse_prefix(len - 1)];
				value -= future_cost[inverse_prefix(previous_match)];

				if(value > best_value){
					match_length = len;
					//printf("%d i2+len\n",(int)(i + len));
					back_ref = yy*width;
					best_value = value;
				}
			}
			else{
				break;
			}
		}
		if(
			best_value == 0
		){
			previous_match++;
			i++;
		}
		else{
			lz_data[lz_size - 1].val_future = previous_match;
			lz_data[lz_size].val_backref = back_ref - 1;
			lz_data[lz_size].val_matchlen = match_length - 1;

			lz_size++;
			previous_match = 0;
			i += match_length;
		}
	}
	lz_data[lz_size - 1].val_future = previous_match;

	delete[] backref_cost;
	delete[] matchlen_cost;
	delete[] future_cost;

	return lz_data;
}

lz_triple* lz_dist_modern(
	uint8_t* in_bytes,
	float* estimate,
	uint32_t width,
	uint32_t height,
	double* backref_cost,
	double* matchlen_cost,
	double* future_cost,
	size_t& lz_size,
	size_t lz_limit
){
	lz_triple* lz_data = new lz_triple[10 + width*height/2];

	size_t previous_match = 0;
	lz_data[0].val_future = 0;
	lz_size = 1;

	bool is_previous_vulnerable = false;
	double previous_value = 0;
	

	for(int i=0;i<width*height;){
		//short range
		double best_insr = 0;
		size_t best_backref = 0;
		size_t best_matchlen = 0;

		size_t backwards_limit = lz_limit;
		if(backwards_limit > 120){
			backwards_limit = 120;
		}
		for(size_t lut_lookup = 0;lut_lookup < backwards_limit;lut_lookup++){
			size_t backref = lut_y[lut_lookup]*width + lut_x[lut_lookup];
			if(backref > i){
				continue;
			}
			for(size_t t=0;i+t < width*height;t++){
				if(
					   in_bytes[(i+t)*3]     != in_bytes[(i+t-backref)*3]
					|| in_bytes[(i+t)*3 + 1] != in_bytes[(i+t-backref)*3 + 1]
					|| in_bytes[(i+t)*3 + 2] != in_bytes[(i+t-backref)*3 + 2]
				){
					if(t > 0){
						double val = 0;
						for(size_t l=0;l<t;l++){
							val += estimate[i + t];
						}
						val -= backref_cost[lut_lookup];
						val -= matchlen_cost[inverse_prefix(t - 1)];
						if(val > best_insr){
							best_insr = val;
							best_backref = backref;
							best_matchlen = t;
						}
					}
					break;
				}
			}
		}
		//up
		backwards_limit = lz_limit;
		if(backwards_limit > 120){
			backwards_limit = 120;
		}
		if(backwards_limit > i/width){
			backwards_limit = i/width;
		}
		for(size_t up = 1;up <= backwards_limit;up++){
			size_t backref = up*width;
			for(size_t t=0;i+t < width*height;t++){
				if(
					   in_bytes[(i+t)*3]     != in_bytes[(i+t-backref)*3]
					|| in_bytes[(i+t)*3 + 1] != in_bytes[(i+t-backref)*3 + 1]
					|| in_bytes[(i+t)*3 + 2] != in_bytes[(i+t-backref)*3 + 2]
				){
					if(t > 0){
						double val = 0;
						for(size_t l=0;l<t;l++){
							val += estimate[i + t];
						}
						val -= backref_cost[140 + inverse_prefix(up - 1)];
						val -= matchlen_cost[inverse_prefix(t - 1)];
						if(val > best_insr){
							best_insr = val;
							best_backref = backref;
							best_matchlen = t;
						}
					}
					break;
				}
			}
		}
		//left
		backwards_limit = lz_limit;
		if(backwards_limit > 120){
			backwards_limit = 120;
		}
		if(backwards_limit > i){
			backwards_limit = i;
		}
		for(size_t backref = 1;backref <= backwards_limit;backref++){
			for(size_t t=0;i+t < width*height;t++){
				if(
					   in_bytes[(i+t)*3]     != in_bytes[(i+t-backref)*3]
					|| in_bytes[(i+t)*3 + 1] != in_bytes[(i+t-backref)*3 + 1]
					|| in_bytes[(i+t)*3 + 2] != in_bytes[(i+t-backref)*3 + 2]
				){
					if(t > 0){
						double val = 0;
						for(size_t l=0;l<t;l++){
							val += estimate[i + t];
						}
						val -= backref_cost[160 + inverse_prefix(backref - 1)];
						val -= matchlen_cost[inverse_prefix(t - 1)];
						if(val > best_insr){
							best_insr = val;
							best_backref = backref;
							best_matchlen = t;
						}
					}
					break;
				}
			}
		}
		//long range
		backwards_limit = lz_limit;
		if(backwards_limit > i){
			backwards_limit = i;
		}
		for(size_t backref = 120;backref <= backwards_limit;backref++){
			for(size_t t=0;i+t < width*height;t++){
				if(
					   in_bytes[(i+t)*3]     != in_bytes[(i+t-backref)*3]
					|| in_bytes[(i+t)*3 + 1] != in_bytes[(i+t-backref)*3 + 1]
					|| in_bytes[(i+t)*3 + 2] != in_bytes[(i+t-backref)*3 + 2]
				){
					if(t > best_matchlen){
						double val = 0;
						for(size_t l=0;l<t;l++){
							val += estimate[i + t];
						}
						val -= backref_cost[160 + inverse_prefix(backref - 1)];
						val -= matchlen_cost[inverse_prefix(t - 1)];
						if(val > best_insr){
							best_insr = val;
							best_backref = backref;
							best_matchlen = t;
						}
					}
					break;
				}
			}
		}
		if(
			best_insr > future_cost[0]
		){
			if(
				is_previous_vulnerable
				&& (
					previous_value
					- future_cost[inverse_prefix(previous_match)]
					- future_cost[inverse_prefix(lz_data[lz_size - 2].val_future)]
					+ future_cost[inverse_prefix(previous_match + lz_data[lz_size - 2].val_future + 1 + lz_data[lz_size - 1].val_matchlen)] <= 0
				)
			){
				if(previous_match == 0){
					lz_size--;
					lz_data[lz_size - 1].val_future += lz_data[lz_size].val_matchlen + 1;
					lz_data[lz_size].val_backref = best_backref - 1;
					lz_data[lz_size].val_matchlen = best_matchlen - 1;

					lz_size++;
					i += best_matchlen;
					previous_value = best_insr;
					is_previous_vulnerable = true;
				}
				else{
					is_previous_vulnerable = false;
					lz_size--;
					i -= lz_data[lz_size].val_matchlen + previous_match;
				}
			}
			else{
				lz_data[lz_size - 1].val_future = previous_match;
				lz_data[lz_size].val_backref = best_backref - 1;
				lz_data[lz_size].val_matchlen = best_matchlen - 1;

				lz_size++;
				previous_match = 0;
				i += best_matchlen;
				previous_value = best_insr;
				is_previous_vulnerable = true;
			}
		}
		else{
			previous_match++;
			i++;
		}
	}
	return lz_data;
}

#endif //LZ_OPTIMISER
