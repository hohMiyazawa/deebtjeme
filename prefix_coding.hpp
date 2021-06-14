#ifndef PREFIX_CODING_HEADER
#define PREFIX_CODING_HEADER

uint8_t val_to_prefix(size_t val){
	if(val < 6){
		return val - 1;
	}
	size_t bound = 2;
	uint8_t counter = 4;
	while(true){
		if(val <= bound*3){
			return counter;
		}
		else if(val <= bound*4){
			return counter + 1;
		}
		bound *= 2;
		counter += 2;
	}
}

uint8_t prefix_to_extra(uint8_t prefix){
	if(prefix < 5){
		return 0;
	}
	return (prefix - 2) >> 1;
}

#endif //PREFIX_CODING
