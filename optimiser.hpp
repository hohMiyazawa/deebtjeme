#ifndef OPTIMISER_HEADER
#define OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_optimiser.hpp"
#include "predictor_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "simple_encoders.hpp"

void encode_ranged_simple2(uint8_t* in_bytes,uint32_t range,uint32_t width,uint32_t height,uint8_t*& outPointer){
	size_t safety_margin = width*height * (log2_plus(range - 1) + 1) + 2048;

	uint8_t alternates = 5;
	uint8_t* miniBuffer[alternates];
	uint8_t* trailing_end[alternates];
	uint8_t* trailing[alternates];
	for(size_t i=0;i<alternates;i++){
		miniBuffer[i] = new uint8_t[safety_margin];
		trailing_end[i] = miniBuffer[i] + safety_margin;
		trailing[i] = trailing_end[i];
	}
	encode_entropy(in_bytes,range,width,height,trailing[0]);
	encode_ffv1(in_bytes,range,width,height,trailing[1]);
	encode_left(in_bytes,range,width,height,trailing[2]);
	encode_top(in_bytes,range,width,height,trailing[3]);
	encode_quad(in_bytes,range,width,height,trailing[4]);
	//research_optimiser_entropyOnly(in_bytes,range,width,height,miniBuffer[2],1);
	for(size_t i=0;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		printf("type %d: %d\n",(int)i,(int)diff);
	}

	uint8_t bestIndex = 0;
	size_t best = trailing_end[0] - trailing[0];
	for(size_t i=1;i<alternates;i++){
		size_t diff = trailing_end[i] - trailing[i];
		if(diff < best){
			best = diff;
			bestIndex = i;
		}
	}
	printf("best type: %d\n",(int)bestIndex);
	for(size_t i=(trailing_end[bestIndex] - trailing[bestIndex]);i--;){
		*(--outPointer) = trailing[bestIndex][i];
	}
	for(size_t i=0;i<alternates;i++){
		delete[] miniBuffer[i];
	}
}

void optimiser_entropyOnly(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats* statistics = new SymbolStats[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		SymbolStats stats;
		statistics[context] = stats;
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropy_image[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		statistics[cntr].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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
///encode data
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context],tableEncode, range);
	}
	delete[] statistics;
	tableEncode.conclude();

	entropyCoding_map(
		filtered_bytes,
		width,
		height,
		table,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(outPointer - trailing));

	*(--outPointer) = 0;
	*(--outPointer) = 0;
	*(--outPointer) = 0;

	if(contextNumber == 1){
		*(--outPointer) = 0;
		*(--outPointer) = 0;
		*(--outPointer) = 0;
		*(--outPointer) = 0b00000100;
	}
	else{
		trailing = outPointer;
		encode_ranged_simple2(
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
		printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

		*(--outPointer) = contextNumber - 1;//number of contexts

		*(--outPointer) = 0;
		*(--outPointer) = 0;
		*(--outPointer) = 0;
		*(--outPointer) = 0b00000110;
	}

	delete[] entropy_image;
}

void optimiser(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats* statistics = new SymbolStats[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		SymbolStats stats;
		statistics[context] = stats;
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropy_image[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		statistics[cntr].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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
/// predictors?
	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);

	size_t available_predictors = 13;

	uint16_t fine_selection[available_predictors] = {
		0,//ffv1
		0b0010000111000001,//(2,1,-1,1)
		0b0001000111010000,//avg L-T
		0b0001000111000000,//(1,1,-1,0)
		0b0011000111000001,//(3,1,-1,1)
		0b0001001111000000,//(1,3,-1,0)
		0b0011000011000010,//(3,0,-1,2)
		0b0011001011000000,//(3,2,-1,0)
		0b0011000011000001,//(3,0,-1,1)
		0b0010000111000000,//(2,1,-1,0)
		0b0001001011000000,//(1,2,-1,0)
		0b0001000011010000,//(1,0,0,0)
		0b0000000111010000//(0,1,0,0)
	};

	printf("testing %d alternate predictors\n",(int)speed);
	for(size_t i=1;i<speed;i++){
		if(i >= available_predictors){
			break;
		}
		predictorCount = add_predictor_maybe(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			fine_selection[i]
		);
	}

	int a_lim = 6;
	int b_lim = 6;
	int c_lim = -4;
	int d_lim = 4;
	if(speed >= 10){
		a_lim++;
		b_lim++;
		c_lim--;
		d_lim++;
	}
	if(speed >= 50){
		a_lim = 16;
		b_lim = 16;
		c_lim = -13;
		d_lim = 12;
	}

	for(int d=0;d<d_lim;d++){
	for(int b=0;b<b_lim;b++){
	for(int c=c_lim;c<3;c++){
	for(int a=0;a<a_lim;a++){
		uint16_t custom_pred = (a << 12) + (b << 8) + ((c + 13) << 4) + d;
		if(!is_valid_predictor(custom_pred)){
			continue;
		}
		if(a == 0 && b == 0 && c < 0){//TR - TL are bad, so don't waste time on them
			continue;
		}
		//printf("trying (%d,%d,%d,%d)\n",a,b,c,d);
		predictorCount = add_predictor_maybe(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			custom_pred
		);
	}
	}
	}
	}

	printf("%d predictors used\n",(int)predictorCount);

	printf("performing %d refinement passes\n",(int)speed);
	for(size_t i=0;i<speed;i++){
		for(size_t entro=0;entro < 5;entro++){
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

	//perform a entropy pass, to get stat tables up to date.
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

///encode data
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context],tableEncode, range);
	}
	delete[] statistics;
	tableEncode.conclude();

	entropyCoding_map(
		filtered_bytes,
		width,
		height,
		table,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	encode_ranged_simple2(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		encode_ranged_simple2(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b00000110;//use both map features
}

void optimiser_speed(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);

	uint8_t* entropy_image;

	uint32_t entropyWidth;
	uint32_t entropyHeight;

	uint8_t contextNumber = entropy_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		entropy_image,
		entropyWidth,
		entropyHeight
	);

	uint32_t entropyWidth_block  = (width + entropyWidth - 1)/entropyWidth;
	uint32_t entropyHeight_block  = (height + entropyHeight - 1)/entropyHeight;
	printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

	SymbolStats* statistics = new SymbolStats[contextNumber];
	
	for(size_t context = 0;context < contextNumber;context++){
		SymbolStats stats;
		statistics[context] = stats;
		for(size_t i=0;i<256;i++){
			statistics[context].freqs[i] = 0;
		}
	}
	for(size_t i=0;i<width*height;i++){
		uint8_t cntr = entropy_image[tileIndexFromPixel(
			i,
			width,
			entropyWidth,
			entropyWidth_block,
			entropyHeight_block
		)];
		statistics[cntr].freqs[filtered_bytes[i]]++;
	}

	printf("performing %d entropy passes\n",(int)speed);
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
/// predictors?
	uint16_t* predictors = new uint16_t[256];
	uint8_t* predictor_image;

	uint32_t predictorWidth;
	uint32_t predictorHeight;

	printf("research: setting initial predictor layout\n");

	uint8_t predictorCount = predictor_map_initial(
		filtered_bytes,
		range,
		width,
		height,
		predictors,
		predictor_image,
		predictorWidth,
		predictorHeight
	);

	size_t available_predictors = 64;

	uint16_t fine_selection[available_predictors] = {
0,
0b0011001011000001,
0b0100000110110001,
0b0100001010110001,
0b0100010010110001,
0b0000001111110010,
0b0001000111000000,
0b0001000111010000,
0b0100001110100001,
0b0011001110110001,
0b0010001011000000,
0b0000000111010000,
0b0001000011010000,
0b0100001111000001,
0b0010001111000001,
0b0100001110110000,
0b0011000111000001,
0b0100010010100000,
0b0100001011000000,
0b0011010011000001,
0b0010010011000000,
0b0011010010110000,
0b0010000111010001,
0b0100000111000001,
0b0011001111000000,
0b0100010011000010,
0b0100000110110010,
0b0001001011010000,
0b0011000111010001,
0b0011001010110001,
0b0001000111010001,
0b0010000111010000,
0b0100010010100010,
0b0010000011000010,
0b0011001010110010,
0b0010000111000001,
0b0010010011010001,
0b0011001110110000,
0b0100000011000010,
0b0100000010110011,
0b0000000111100001,
0b0010010010110001,
0b0100001110110011,
0b0010000111000011,
0b0001010011000001,
0b0001010011010000,
0b0000001011100001,
0b0000010011100010,
0b0000010011100001,
0b0001001011010001,
0b0011001111010001,
0b0001000111100000,
0b0100001010100010,
0b0100000111010000,
0b0010000111100001,
0b0000001111100001,
0b0010010011000010,
0b0011010011010001,
0b0011001011010001,
0b0000000011010001,
0b0001000011010001,
0b0100000111010001,
0b0011000011000011,
0b0000010011100000
	};

	printf("testing %d alternate predictors\n",(int)speed);
	for(size_t i=1;i<available_predictors;i++){
		predictorCount = add_predictor_maybe(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			fine_selection[i]
		);
	}

	printf("%d predictors used\n",(int)predictorCount);

	printf("performing %d refinement passes\n",(int)speed);
	for(size_t i=0;i<speed;i++){
		for(size_t entro=0;entro < 5;entro++){
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

		//printf("shuffling predictors around\n");
		double saved = predictor_redistribution_pass(
			in_bytes,
			filtered_bytes,
			range,
			width,
			height,
			entropy_image,
			contextNumber,
			entropyWidth,
			entropyHeight,
			statistics,
			predictors,
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight
		);
		printf("saved: %f bits\n",saved);
		if(saved < 8){//early escape
			break;
		}
	}

	//perform a entropy pass, to get stat tables up to date.
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

///encode data
	BitWriter tableEncode;
	SymbolStats table[contextNumber];
	for(size_t context = 0;context < contextNumber;context++){
		table[context] = encode_freqTable(statistics[context],tableEncode, range);
	}
	delete[] statistics;
	tableEncode.conclude();

	entropyCoding_map(
		filtered_bytes,
		width,
		height,
		table,
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);

	delete[] filtered_bytes;

	uint8_t* trailing = outPointer;
	for(size_t i=tableEncode.length;i--;){
		*(--outPointer) = tableEncode.buffer[i];
	}
	printf("entropy table size: %d bytes\n",(int)(trailing - outPointer));

	trailing = outPointer;
	encode_ranged_simple2(
		entropy_image,
		contextNumber,
		entropyWidth,
		entropyHeight,
		outPointer
	);
	delete[] entropy_image;
	writeVarint_reverse((uint32_t)(entropyHeight - 1),outPointer);
	writeVarint_reverse((uint32_t)(entropyWidth - 1), outPointer);
	printf("entropy image size: %d bytes\n",(int)(trailing - outPointer));

	*(--outPointer) = contextNumber - 1;//number of contexts

	if(predictorCount > 1){
		uint8_t* trailing = outPointer;
		encode_ranged_simple2(
			predictor_image,
			predictorCount,
			predictorWidth,
			predictorHeight,
			outPointer
		);
		writeVarint_reverse((uint32_t)(predictorHeight - 1),outPointer);
		writeVarint_reverse((uint32_t)(predictorWidth - 1), outPointer);
		printf("predictor image size: %d bytes\n",(int)(trailing - outPointer));
	}
	delete[] predictor_image;

	for(size_t i=predictorCount;i--;){
		*(--outPointer) = predictors[i] % 256;
		*(--outPointer) = predictors[i] >> 8;
	}
	delete[] predictors;
	*(--outPointer) = predictorCount - 1;

	*(--outPointer) = 0b00000110;//use both map features
}
#endif //OPTIMISER
