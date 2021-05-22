#ifndef VARINT_HEADER
#define VARINT_HEADER

#include <assert.h>

uint32_t readVarint(uint8_t*& pointer){
	uint32_t value = 0;
	uint32_t read1 = *(pointer++);
	if(read1 < 128){
		return read1;
	}
	else{
		uint32_t read2 = *(pointer++);
		if(read2 < 128){
			return (read1 % 128) + (read2 << 7);
		}
		else{
			uint32_t read3 = *(pointer++);
			if(read3 < 128){
				return (read1 % 128) + ((read2 % 128) << 7)  + (read3 << 14);
			}
			else{
				uint32_t read4 = *(pointer++);
				return (read1 % 128) + ((read2 % 128) << 7)  + ((read3 % 128) << 14) + (read4 << 21);
			}
		}
	}
}

void writeVarint(uint32_t value,uint8_t*& outPointer){
	assert(value < (1 << 28));
	if(value < 128){
		*(outPointer++) = (uint8_t)value;
	}
	else if(value < (1<<14)){
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 7);
	}
	else if(value < (1<<21)){
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 7) % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 14);

	}
	else{
		*(outPointer++) = (uint8_t)((value % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 7) % 128) + 128);
		*(outPointer++) = (uint8_t)(((value >> 14) % 128) + 128);
		*(outPointer++) = (uint8_t)(value >> 21);
	}
}

#endif //VARINT_HEADER
