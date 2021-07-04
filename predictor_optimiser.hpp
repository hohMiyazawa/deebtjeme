#ifndef PREDICTOR_OPTIMISER_HEADER
#define PREDICTOR_OPTIMISER_HEADER

/*
	heuristic for predictor mapping
*/
uint8_t predictor_map_initial(
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

	predictor_image = new uint8_t[predictorWidth*predictorHeight];
	for(size_t i=0;i<predictorWidth*predictorHeight;i++){
		predictor_image[i] = 0;
	}
	predictors[0] = 0;

	return 1;
}

uint32_t add_predictor_maybe(
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

	double* costTables[contexts];

	for(size_t i=0;i<contexts;i++){
		costTables[i] = entropyLookup(entropy_stats[i]);
	}

	uint8_t* filter2 = filter_all(in_bytes, range, width, height, predictor);

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
		cost_per_occurence = -std::log2((double)blocks/(double)(predictor_width*predictor_height));
		saved = 0;
		blocks = 0;
		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
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
				if(alternate + cost_per_occurence < native){
					blocks++;
					saved += native - (alternate + cost_per_occurence);
					predictor_image[i] = predictor_count;
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
							filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
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

uint32_t add_predictor_maybe_prefiltered(
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
	uint16_t predictor,
	uint8_t**& filter_collection
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

	uint8_t* filter2 = filter_all(in_bytes, range, width, height, predictor);

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
		printf(" %f",saved);
		cost_per_occurence = -std::log2((double)blocks/(double)(predictor_width*predictor_height));
		saved = 0;
		blocks = 0;
		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
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
				if(alternate + cost_per_occurence < native){
					blocks++;
					saved += native - (alternate + cost_per_occurence);
					predictor_image[i] = predictor_count;
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
							filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
						}
					}
				}
			}
		}
		else{
			//nonaligned block? how to deal with that?
		}
		predictors[predictor_count] = predictor;
		filter_collection[predictor_count] = filter2;
		predictor_count++;
	}
	else{
		delete[] filter2;
	}

//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}
	return predictor_count;
}

double predictor_redistribution_pass(
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
	for(size_t i=0;i<predictor_width*predictor_height;i++){
		used_count[predictor_image[i]]++;
	}

	double pass_saved = 0;

	for(size_t pred=0;pred<predictor_count;pred++){
		uint8_t* filter2 = filter_all(in_bytes, range, width, height, predictors[pred]);

		double cost_per_occurence = -std::log2((double)(used_count[pred])/((double(predictor_width*predictor_height))));

		double saved = 0;
		size_t blocks = 0;

		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
				double native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block
				);
				double native_cost_per_occurence = -std::log2((double)(used_count[predictor_image[i]])/((double(predictor_width*predictor_height))));
				double alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block
				);
				if(alternate + cost_per_occurence < native + native_cost_per_occurence){
					blocks++;
					saved += native + native_cost_per_occurence - (alternate + cost_per_occurence);
					predictor_image[i] = pred;
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
							filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
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

double predictor_redistribution_pass_prefiltered(
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
	uint8_t** filter_collection
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
	for(size_t i=0;i<predictor_width*predictor_height;i++){
		used_count[predictor_image[i]]++;
	}

	double pass_saved = 0;

	for(size_t pred=0;pred<predictor_count;pred++){
		uint8_t* filter2 = filter_collection[pred];

		double cost_per_occurence = -std::log2((double)(used_count[pred])/((double(predictor_width*predictor_height))));

		double saved = 0;
		size_t blocks = 0;

		if(
			entropy_width == predictor_width
			&& entropy_height == predictor_height
		){
			for(size_t i=0;i<predictor_width*predictor_height;i++){
				double native = regionalEntropy(
					filtered_bytes,
					costTables[entropy_image[i]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block
				);
				double native_cost_per_occurence = -std::log2((double)(used_count[predictor_image[i]])/((double(predictor_width*predictor_height))));
				double alternate = regionalEntropy(
					filter2,
					costTables[entropy_image[i]],
					i,
					width,
					height,
					predictor_width_block,
					predictor_height_block
				);
				if(alternate + cost_per_occurence < native + native_cost_per_occurence){
					blocks++;
					saved += native + native_cost_per_occurence - (alternate + cost_per_occurence);
					predictor_image[i] = pred;
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
							filtered_bytes[y_pos * width + x_pos] = filter2[y_pos * width + x_pos];
						}
					}
				}
			}
		}
		else{
			//nonaligned block? how to deal with that?
		}

		pass_saved += saved;
		
	}
//free memory
	for(size_t i=0;i<contexts;i++){
		delete[] costTables[i];
	}

	return pass_saved;
}

uint16_t fine_selection[128] = {
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
0b0000010011100000,
0b0100001011000010,
0b0011010010110010,
0b0100001011010001,
0b0001010011010001,
0b0011001110110011,
0b0000000111100000,
0b0001000011100000,
0b0011000111000010,
0b0011001011010000,
0b0001000111110000,
0b0100001110100011,
0b0001000011100001,
0b0100010011010001,
0b0000000111110000,
0b0001001011100000,
0b0011010010110011,
0b0000000011100000,
0b0011010010100001,
0b0100000011100000,
0b0001001111010000,
0b0011001111000010,
0b0100000011010001,
0b0010000011010001,
0b0000001111110000,
0b0010001111010000,
0b0001001011000001,
0b0000001011110001,
0b0000001011100000,
0b0001000011110000,
0b0001001111000001,
0b0010000111110000,
0b0011000111010000,
0b0001001011110000,
0b0010000011100000,
0b0100001110010001,
0b0000010011110001,
0b0001001011000011,
0b0100010010010001,
0b0100001110110010,
0b0010000111100000,
0b0001010011100000,
0b0001010011000011,
0b0010001111010001,
0b0010001011100001,
0b0100001010110011,
0b0000010011010001,
0b0000000111010001,
0b0010010011100000,
0b0011000111100001,
0b0011010011010000,
0b0010001011010001,
0b0011000011010001,
0b0011000011110000,
0b0001000111100001,
0b0010001110110001,
0b0100001111100001,
0b0010010010110011,
0b0001000111000011,
0b0001001111010001,
0b0011000111100000,
0b0001001111110000,
0b0100010011110011,
0b0100010011000000,
0b0001001111000010
};

#endif //PREDICTOR_OPTIMISER
