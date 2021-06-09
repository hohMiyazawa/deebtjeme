#ifndef RESEARCH_OPTIMISER_HEADER
#define RESEARCH_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "rans_byte.h"
#include "entropy_optimiser.hpp"
#include "predictor_optimiser.hpp"
#include "entropy_coding.hpp"
#include "table_encode.hpp"
#include "2dutils.hpp"
#include "bitwriter.hpp"
#include "simple_encoders.hpp"

void research_optimiser(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& outPointer,
	size_t speed
){
	uint8_t* filtered_bytes = filter_all_ffv1(in_bytes, range, width, height);
	uint8_t* ffv1_bytes = filter_all_ffv1(in_bytes, range, width, height);

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
	//printf("entropy dimensions %d x %d\n",(int)entropyWidth,(int)entropyHeight);

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

	//printf("performing %d entropy passes\n",(int)speed);
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

	//printf("research: setting initial predictor layout\n");

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
	uint32_t predictor_width_block  = (width + predictorWidth - 1)/predictorWidth;
	uint32_t predictor_height_block = (height + predictorHeight - 1)/predictorHeight;

	size_t available_predictors = 13;

	//not used, may come in handy later
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

	double* costTables[contextNumber];
	for(size_t i=0;i<contextNumber;i++){
		costTables[i] = entropyLookup(statistics[i]);
	}

	double pre_savings[1 << 16];
	size_t pre_blocks[1 << 16];
	for(size_t i=0;i<(1 << 16);i++){
		pre_savings[i] = 0;
		pre_blocks[i] = 0;
	}
	pre_blocks[0] = predictorWidth*predictorHeight;

	double local_savings[predictorWidth*predictorHeight];
	uint16_t local_predictor[predictorWidth*predictorHeight];
	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		local_savings[i] = 0;
		local_predictor[i] = 0;
	}


	int a_lim = 5;
	int b_lim = 5;
	int c_lim = -5;
	int d_lim = 4;

	size_t testing = 0;

	for(int d=0;d<d_lim;d++){
	for(int b=0;b<b_lim;b++){
	for(int c=c_lim;c<3;c++){
	for(int a=0;a<a_lim;a++){
		uint16_t custom_pred = (a << 12) + (b << 8) + ((c + 13) << 4) + d;
		if(a == 0 && b == 0 && c == -1 && d == 00){
			custom_pred = 0;
		}
		if(!is_valid_predictor(custom_pred)){
			continue;
		}
		if(a == 0 && b == 0 && c < 0 && d > 0){//TR - TL are bad, so don't waste time on them
			continue;
		}
///
		testing++;
		//printf("(%d,%d,%d,%d)\n",a,b,c,d);
		uint8_t* filter2 = filter_all(in_bytes, range, width, height, custom_pred);
		for(size_t i=0;i<predictorWidth*predictorHeight;i++){
			double native = regionalEntropy(
				filtered_bytes,
				costTables[entropy_image[i]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block
			);
			double alternate = regionalEntropy(
				filter2,
				costTables[entropy_image[i]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block
			);
			if(alternate < native){
				double old_savings_here = local_savings[i];
				local_savings[i] = old_savings_here + native - alternate;
				pre_savings[local_predictor[i]] -= old_savings_here;
				pre_blocks[local_predictor[i]]--;
				local_predictor[i] = custom_pred;
				pre_blocks[custom_pred]++;
				pre_savings[custom_pred] += old_savings_here + native - alternate;
				for(size_t y = 0;y < predictor_height_block;y++){
					size_t y_pos = (i / predictorWidth) * predictor_height_block + y;
					if(y_pos >= height){
						break;
					}
					for(size_t x = 0;x < predictor_width_block;x++){
						size_t x_pos = (i % predictorWidth) * predictor_width_block + x;
						if(x_pos >= width){
							continue;
						}
						filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
					}
				}
			}
		}
///
		delete[] filter2;
///
	}
	}
	}
	}


for(size_t loop=0;loop < 8;loop++){


//now we have a pretty good idea of prediction performance (slightly too optimistic, in fact):
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
//
//now for round two:

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
		costTables[i] = entropyLookup(statistics[i]);
	}

	size_t post_blocks[1 << 16];
	for(size_t i=0;i<(1 << 16);i++){
		post_blocks[i] = 0;
	}
	post_blocks[0] = predictorWidth*predictorHeight;
/*
	double local_savings[predictorWidth*predictorHeight];
	uint16_t local_predictor[predictorWidth*predictorHeight];
*/

	double cost_per_occurence_ffv1 = -std::log2((double)(pre_blocks[0])/((double(predictorWidth*predictorHeight))));

	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		local_savings[i] = -cost_per_occurence_ffv1;
		local_predictor[i] = 0;
	}


	for(int d=0;d<d_lim;d++){
	for(int b=0;b<b_lim;b++){
	for(int c=c_lim;c<3;c++){
	for(int a=0;a<a_lim;a++){
		uint16_t custom_pred = (a << 12) + (b << 8) + ((c + 13) << 4) + d;
		if(a == 0 && b == 0 && c == -1 && d == 0){
			custom_pred = 0;
		}
		if(!is_valid_predictor(custom_pred)){
			continue;
		}
		if(a == 0 && b == 0 && c < 0 && d > 0){//TR - TL are bad, so don't waste time on them
			continue;
		}
///
		if(pre_blocks[custom_pred] < 4){
			continue;
		}
		//printf("(%d,%d,%d,%d)\n",a,b,c,d);
		uint8_t* filter2 = filter_all(in_bytes, range, width, height, custom_pred);
		double cost_per_occurence = -std::log2((double)(pre_blocks[custom_pred])/((double(predictorWidth*predictorHeight))));

		for(size_t i=0;i<predictorWidth*predictorHeight;i++){
			double very_basic = regionalEntropy(
				ffv1_bytes,
				costTables[entropy_image[i]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block
			);
			double alternate = regionalEntropy(
				filter2,
				costTables[entropy_image[i]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block
			);
			//more sophisticated compare
			double basic_gain = very_basic - alternate - cost_per_occurence;

			if(
				basic_gain > local_savings[i]
			){
				local_savings[i] = basic_gain;
				post_blocks[local_predictor[i]]--;
				local_predictor[i] = custom_pred;
				post_blocks[custom_pred]++;

				for(size_t y = 0;y < predictor_height_block;y++){
					size_t y_pos = (i / predictorWidth) * predictor_height_block + y;
					if(y_pos >= height){
						break;
					}
					for(size_t x = 0;x < predictor_width_block;x++){
						size_t x_pos = (i % predictorWidth) * predictor_width_block + x;
						if(x_pos >= width){
							continue;
						}
						filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
					}
				}
			}
		}
///
		delete[] filter2;
///
	}
	}
	}
	}

	for(size_t i=0;i<(1 << 16);i++){
		pre_blocks[i] = post_blocks[i];
	}
}
	size_t count = 0;
	for(int d=0;d<d_lim;d++){
	for(int b=0;b<b_lim;b++){
	for(int c=c_lim;c<3;c++){
	for(int a=0;a<a_lim;a++){
		uint16_t custom_pred = (a << 12) + (b << 8) + ((c + 13) << 4) + d;
		if(a == 0 && b == 0 && c == -1 && d == 0){
			custom_pred = 0;
		}
		if(!is_valid_predictor(custom_pred)){
			continue;
		}
		if(a == 0 && b == 0 && c < 0 && d > 0){//TR - TL are bad, so don't waste time on them
			continue;
		}
		if(pre_blocks[custom_pred] > 8){
			count++;
			printf("['(%d,%d,%d,%d)', %d],\n",a,b,c,d,(int)pre_blocks[custom_pred]);
		}
	}
	}
	}
	}
	//printf("count %d\n",(int)count);

	for(size_t i=0;i<contextNumber;i++){
		delete[] costTables[i];
	}
	delete[] predictors;
	delete[] predictor_image;
	delete[] entropy_image;
	delete[] filtered_bytes;
}

#endif //RESEARCH_OPTIMISER
