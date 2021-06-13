#ifndef LZ_OPTIMISER_HEADER
#define LZ_OPTIMISER_HEADER

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
			}
		}
		size_t y = i/width;
		if(y){
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
			i++;
			previous_match++;
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
	return lz_data;
}

#endif //LZ_OPTIMISER
