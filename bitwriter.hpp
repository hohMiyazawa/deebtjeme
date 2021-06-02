#ifndef BITWRITER_HEADER
#define BITWRITER_HEADER

#include "panic.hpp"

class BitWriter{
	public:
		uint8_t buffer[65536];
		size_t length;
		BitWriter();
		void writeBits(uint8_t value,uint8_t size);
		void conclude();
		
	private:
		uint8_t partial;
		uint8_t partial_length;
};

BitWriter::BitWriter(){
	length = 0;
	partial = 0;
	partial_length = 0;
}
void BitWriter::conclude(){
	if(partial_length){
		buffer[length++] = partial;
	}
	partial = 0;
	partial_length = 0;
}
void BitWriter::writeBits(uint8_t value,uint8_t size){
	if(size == 0){
	}
	else if(size > 8){
		panic("can't write more than 8bits at the time to a buffer!");
	}
	else if(size + partial_length == 8){
		buffer[length++] = partial + value;
		partial = 0;
		partial_length = 0;
	}
	else if(size + partial_length < 8){
		partial += value << (8 - partial_length - size);
		partial_length += size;
	}
	else{
		buffer[length++] = partial + (value >> (partial_length + size - 8));
		partial = (value % (1 << (partial_length + size - 8))) << (16 - partial_length - size);
		partial_length = (partial_length + size - 8);
	}
}

#endif //BITWRITER_HEADER
