#ifndef INDEX_ENCODER_HEADER
#define INDEX_ENCODER_HEADER

#include "simple_encoders.hpp"
#include "colour_simple_encoders.hpp"

void indexed_encode_speed0(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer
){
	uint8_t in_bytes[width*height];
	for(size_t i=0;i<width*height;i++){
		for(size_t j=0;j<colour_count;j++){
			if(
				palette[j*3] == raw_bytes[i*3]
				&& palette[j*3+1] == raw_bytes[i*3+1]
				&& palette[j*3+2] == raw_bytes[i*3+2]
			){
				in_bytes[i] = j;
				break;
			}
		}
	}
	SymbolStats stats;
	stats.count_freqs(in_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode, colour_count);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + in_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);

	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	*(--outPointer) = 0;

	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;

	*(--outPointer) = 0b00110010;
}

void indexed_encode_speed1(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer
){
	uint8_t in_bytes[width*height];
	for(size_t i=0;i<width*height;i++){
		for(size_t j=0;j<colour_count;j++){
			if(
				palette[j*3] == raw_bytes[i*3]
				&& palette[j*3+1] == raw_bytes[i*3+1]
				&& palette[j*3+2] == raw_bytes[i*3+2]
			){
				in_bytes[i] = j;
				break;
			}
		}
	}
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, colour_count, width, height);

	SymbolStats stats;
	stats.count_freqs(filtered_bytes, width*height);

	BitWriter tableEncode;
	SymbolStats table = encode_freqTable(stats,tableEncode, colour_count);
	tableEncode.conclude();

	RansEncSymbol esyms[256];

	for(size_t i=0; i < 256; i++) {
		RansEncSymbolInit(&esyms[i], table.cum_freqs[i], table.freqs[i], 16);
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		RansEncPutSymbol(&rans, &outPointer, esyms + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));
	*(--outPointer) = 1 - 1;

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = 0b00110110;
}

void indexed_encode_speed2(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t in_bytes[width*height];
	for(size_t i=0;i<width*height;i++){
		for(size_t j=0;j<colour_count;j++){
			if(
				palette[j*3] == raw_bytes[i*3]
				&& palette[j*3+1] == raw_bytes[i*3+1]
				&& palette[j*3+2] == raw_bytes[i*3+2]
			){
				in_bytes[i] = j;
				break;
			}
		}
	}
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, colour_count, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	printf("initial distribution\n");
	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight,
		16,16,4
	);
	printf("contexts: %d\n",(int)contextNumber);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats statistics[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		size_t tile_index = tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		statistics[entropy_image[tile_index]].freqs[filtered_bytes[i]]++;
	}

	for(size_t i=0;i<speed;i++){
		contextNumber = entropy_redistribution_pass(
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics
		);
	}

	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context], tableEncode, 2);
	}
	tableEncode.conclude();

	RansEncSymbol esyms[contextNumber][256];

	for(size_t cont=0;cont<contextNumber;cont++){
		for(size_t i=0; i < 256; i++) {
			RansEncSymbolInit(&esyms[cont][i], table[cont].cum_freqs[i], table[cont].freqs[i], 16);
		}
	}

	RansState rans;
	RansEncInit(&rans);
	for(size_t index=width*height;index--;){
		size_t tile_index = tileIndexFromPixel(
			index,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		);
		RansEncPutSymbol(&rans, &outPointer, esyms[entropy_image[tile_index]] + filtered_bytes[index]);
	}
	RansEncFlush(&rans, &outPointer);
	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	if(contextNumber > 1){
		encode_entropy(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] entropy_image;

	trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	printf("contexts: %d\n",(int)contextNumber);
	*(--outPointer) = contextNumber - 1;//number of contexts

	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0b00000000;
	*(--outPointer) = 0;

	trailing = outPointer;
	colour_encode_raw(palette, colour_count, colour_count, 1,  outPointer);
	*(--outPointer) = 0;
	*(--outPointer) = colour_count - 1;
	printf("palette size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = 0b00110110;
}

void indexed_encode(
	uint8_t* raw_bytes,
	uint32_t range,
	uint8_t* palette,
	size_t colour_count,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	if(speed == 0){
		indexed_encode_speed0(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer
		);
	}
	else if(speed == 1){
		indexed_encode_speed1(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer
		);
	}
	else{
		indexed_encode_speed2(
			raw_bytes,
			range,
			palette,
			colour_count,
			width,
			height,
			outPointer,
			speed
		);
	}
}

#endif //INDEX_ENCODER_HEADER
