#ifndef DELTA_COLOUR_HEADER
#define DELTA_COLOUR_HEADER

uint16_t delta(uint8_t transform,uint8_t green){
	return ((uint16_t)green) * ((uint16_t)transform + 1) >> 8;
}

#endif // DELTA_COLOUR_HEADER
