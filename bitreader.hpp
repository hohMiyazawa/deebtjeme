#ifndef BITREADER_HEADER
#define BITREADER_HEADER

class BitReader{
	public:
		BitReader(uint8_t** source);
		BitReader(uint8_t** source,uint8_t startBits,uint8_t startBits_number);
		uint8_t readBits(uint8_t size);
	private:
		uint8_t partial;
		uint8_t partial_length;
		uint8_t** byteSource;
};

BitReader::BitReader(uint8_t** source){
	partial = 0;
	partial_length = 0;
	byteSource = source;
}

BitReader::BitReader(uint8_t** source,uint8_t startBits,uint8_t startBits_number){
	partial = startBits;
	partial_length = startBits_number;
	byteSource = source;
}

uint8_t BitReader::readBits(uint8_t size){
	if(size == 0){
		return 0;
	}
	else if(size == partial_length){
		uint8_t result = partial;
		partial = 0;
		partial_length = 0;
		return result;
	}
	else if(size < partial_length){
		uint8_t result = partial >> (partial_length - size);
		partial = partial - (result << (partial_length - size));
		partial_length -= size;
		return result;
	}
	else{
		uint8_t result = partial << (size - partial_length);
		size -= partial_length;
		partial = **byteSource;
		(*byteSource)++;
		partial_length = 8;

		result += partial >> (partial_length - size);
		partial = partial - (result << (partial_length - size));
		partial_length -= size;

		return result;
	}
}

#endif //BITREADER_HEADER
