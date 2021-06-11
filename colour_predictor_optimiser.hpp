#ifndef COLOUR_PREDICTOR_OPTIMISER_HEADER
#define PREDICTOR_OPTIMISER_HEADER

#include "symbolstats.hpp"
#include "colour_filters.hpp"
#include "entropy_estimation.hpp"

uint8_t clamp(int a,uint32_t range){
	if(a < 0){
		return 0;
	}
	else if(a > (range - 1)){
		return (range - 1);
	}
	else{
		return (uint8_t)a;
	}
}

/*
	heuristic for predictor mapping
*/
uint8_t colour_predictor_map_initial(
	uint8_t* in_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint16_t*& predictors,
	uint8_t*& predictor_image,
	uint32_t& predictorWidth,
	uint32_t& predictorHeight
){
	uint32_t predictorWidth_block  = 8;
	uint32_t predictorHeight_block = 8;

	predictorWidth  = (width + predictorWidth_block - 1)/predictorWidth_block;
	predictorHeight = (height + predictorHeight_block - 1)/predictorHeight_block;

	predictor_image = new uint8_t[predictorWidth*predictorHeight*3];
	for(size_t i=0;i<predictorWidth*predictorHeight*3;i++){
		predictor_image[i] = 0;
	}
	predictors[0] = 0;

	return 1;
}

uint32_t colourSub_add_predictor_maybe(
	uint8_t* in_bytes,
	uint8_t* filtered_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& entropy_image,
	uint32_t contexts,
	uint32_t entropy_width,
	uint32_t entropy_height,
	SymbolStats* entropy_stats,
	uint16_t*& predictors,
	uint8_t*& predictor_image,
	uint32_t predictor_count,
	uint32_t predictor_width,
	uint32_t predictor_height,
	uint16_t predictor
){
	if(predictor_count >= 255){
		printf("too many predictors, try trimming\n");
		return predictor_count;
	}
	for(size_t i=0;i<predictor_count;i++){
		if(predictors[i] == predictor){
			return predictor_count;
		}
	}
	uint32_t predictor_width_block  = (width + predictor_width - 1)/predictor_width;
	uint32_t predictor_height_block = (height + predictor_height - 1)/predictor_height;
	uint32_t entropy_width_block  = (width + entropy_width - 1)/entropy_width;
	uint32_t entropy_height_block = (height + entropy_height - 1)/entropy_height;

	double* costTables[contexts];

	for(size_t i=0;i<contexts;i++){
		costTables[i] = entropyLookup(entropy_stats[i]);
	}

	uint8_t* filter2 = colourSub_filter_all(in_bytes, range, width, height, predictor);

	double cost_per_occurence = std::log2((double)predictor_count);
	double saved = 0;
	size_t blocks = 0;

	if(
		entropy_width == predictor_width
		&& entropy_height == predictor_height
	){
		for(size_t i=0;i<predictor_width*predictor_height;i++){
			double native = regionalEntropy(
				filtered_bytes,
				costTables[entropy_image[i*3]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				0
			);
			double alternate = regionalEntropy(
				filter2,
				costTables[entropy_image[i*3]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				0
			);
			if(alternate + cost_per_occurence < native){
				blocks++;
				saved += native - (alternate + cost_per_occurence);
			}


			native = regionalEntropy(
				filtered_bytes,
				costTables[entropy_image[i*3 + 1]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				1
			);
			alternate = regionalEntropy(
				filter2,
				costTables[entropy_image[i*3 + 1]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				1
			);
			if(alternate + cost_per_occurence < native){
				blocks++;
				saved += native - (alternate + cost_per_occurence);
			}
			native = regionalEntropy(
				filtered_bytes,
				costTables[entropy_image[i*3 + 2]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				2
			);
			alternate = regionalEntropy(
				filter2,
				costTables[entropy_image[i*3 + 2]],
				i,
				width,
				height,
				predictor_width_block,
				predictor_height_block,
				2
			);
			if(alternate + cost_per_occurence < native){
				blocks++;
				saved += native - (alternate + cost_per_occurence);
			}
		}
	}
	else{
		//nonaligned block? how to deal with that?
	}
	if(blocks && saved > 80){
		printf("maybe: %d %f\n",(int)blocks,saved);
		cost_per_occurence = -std::log2((double)blocks/(double)(predictor_width*predictor_height*3));
		saved = 0;
		blocks = 0;
		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
				double native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					0
				);
				double alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					0
				);
				if(alternate + cost_per_occurence < native){
					blocks++;
					saved += native - (alternate + cost_per_occurence);
					predictor_image[i*3] = predictor_count;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3] = filter2[(y_pos * width + x_pos)*3];
						}
					}
				}


				native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3 + 1]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					1
				);
				alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3 + 1]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					1
				);
				if(alternate + cost_per_occurence < native){
					blocks++;
					saved += native - (alternate + cost_per_occurence);
					predictor_image[i*3 + 1] = predictor_count;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3 + 1] = filter2[(y_pos * width + x_pos)*3 + 1];
						}
					}
				}
				native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3 + 2]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					2
				);
				alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3 + 2]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					2
				);
				if(alternate + cost_per_occurence < native){
					blocks++;
					saved += native - (alternate + cost_per_occurence);
					predictor_image[i*3 + 2] = predictor_count;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3 + 2] = filter2[(y_pos * width + x_pos)*3 + 2];
						}
					}
				}
			}
		}
		else{
			//nonaligned block? how to deal with that?
		}
		predictors[predictor_count] = predictor;
		predictor_count++;
	}

//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}
	delete[] filter2;
	return predictor_count;
}

double colourSub_predictor_redistribution_pass(
	uint8_t* in_bytes,
	uint8_t* filtered_bytes,
	uint32_t range,
	uint32_t width,
	uint32_t height,
	uint8_t*& entropy_image,
	uint32_t contexts,
	uint32_t entropy_width,
	uint32_t entropy_height,
	SymbolStats* entropy_stats,
	uint16_t*& predictors,
	uint8_t*& predictor_image,
	uint32_t predictor_count,
	uint32_t predictor_width,
	uint32_t predictor_height
){
	uint32_t predictor_width_block  = (width + predictor_width - 1)/predictor_width;
	uint32_t predictor_height_block = (height + predictor_height - 1)/predictor_height;
	uint32_t entropy_width_block  = (width + entropy_width - 1)/entropy_width;
	uint32_t entropy_height_block = (height + entropy_height - 1)/entropy_height;

	double* costTables[contexts];

	for(size_t i=0;i<contexts;i++){
		costTables[i] = entropyLookup(entropy_stats[i]);
	}


	size_t used_count[predictor_count];
	for(size_t i=0;i<predictor_count;i++){
		used_count[i] = 0;
	}
	for(size_t i=0;i<predictor_width*predictor_height*3;i++){
		used_count[predictor_image[i]]++;
	}

	double pass_saved = 0;

	for(size_t pred=0;pred<predictor_count;pred++){
		uint8_t* filter2 = colourSub_filter_all(in_bytes, range, width, height, predictors[pred]);

		double cost_per_occurence = -std::log2((double)(used_count[pred])/((double(predictor_width*predictor_height*3))));

		double saved = 0;
		size_t blocks = 0;

		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
				double native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					0
				);
				double native_cost_per_occurence = -std::log2((double)(used_count[predictor_image[i*3]])/((double(predictor_width*predictor_height*3))));
				double alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					0
				);
				if(alternate + cost_per_occurence < native + native_cost_per_occurence){
					blocks++;
					saved += native + native_cost_per_occurence - (alternate + cost_per_occurence);
					predictor_image[i*3] = pred;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3] = filter2[(y_pos * width + x_pos)*3];
						}
					}
				}


				native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3+1]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					1
				);
				native_cost_per_occurence = -std::log2((double)(used_count[predictor_image[i*3+1]])/((double(predictor_width*predictor_height*3))));
				alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3+1]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					1
				);
				if(alternate + cost_per_occurence < native + native_cost_per_occurence){
					blocks++;
					saved += native + native_cost_per_occurence - (alternate + cost_per_occurence);
					predictor_image[i*3+1] = pred;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3+1] = filter2[(y_pos * width + x_pos)*3+1];
						}
					}
				}
				native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i*3+2]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					2
				);
				native_cost_per_occurence = -std::log2((double)(used_count[predictor_image[i*3+2]])/((double(predictor_width*predictor_height*3))));
				alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i*3+2]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block,
					2
				);
				if(alternate + cost_per_occurence < native + native_cost_per_occurence){
					blocks++;
					saved += native + native_cost_per_occurence - (alternate + cost_per_occurence);
					predictor_image[i*3+2] = pred;
					for(size_t y = 0;y < predictor_height_block;y++){
						size_t y_pos = (i / predictor_width) * predictor_height_block + y;
						if(y_pos >= height){
							break;
						}
						for(size_t x = 0;x < predictor_width_block;x++){
							size_t x_pos = (i % predictor_width) * predictor_width_block + x;
							if(x_pos >= width){
								continue;
							}
							filtered_bytes[(y_pos * width + x_pos)*3+2] = filter2[(y_pos * width + x_pos)*3+2];
						}
					}
				}
			}
		}
		else{
			//nonaligned block? how to deal with that?
		}

		pass_saved += saved;

		delete[] filter2;
		
	}
//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}

	return pass_saved;
}

#endif //COLOUR_PREDICTOR_OPTIMISER
