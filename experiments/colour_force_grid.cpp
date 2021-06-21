#ifndef LODE_IO_HEADER
#define LODE_IO_HEADER

#include "../lodepng.h"
#include "../delta_colour.hpp"
#include "../colour_filters.hpp"
#include "../entropy_estimation.hpp"
#include <iostream>

void encodeOneStep(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height) {
	//Encode the image
	unsigned error = lodepng::encode(filename, image, width, height);

	//if there's an error, display it
	if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
}

uint8_t* decodeOneStep(const char* filename,unsigned* width_out,unsigned* height_out) {
	std::vector<unsigned char> image; //the raw pixels
	unsigned width, height;

	//decode
	unsigned error = lodepng::decode(image, width, height, filename);

	*width_out = width;
	*height_out = height;

	//if there's an error, display it
	if(error){
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
	}


	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
	uint8_t* a = new uint8_t[width*height*8];
	std::copy(image.begin(), image.end(), a);

	return a;
}

#endif //LODE_IO_HEADER

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		return 1;
	}

	unsigned width = 0, height = 0;

	printf("reading png\n");
	uint8_t* decoded = decodeOneStep(argv[1],&width,&height);
	printf("width : %d\n",(int)width);
	printf("height: %d\n",(int)height);

	size_t blockSize = atoi(argv[2]);
	size_t block_x = (width + blockSize - 1)/blockSize;
	size_t block_y = (height + blockSize - 1)/blockSize;

	double best = 0;

	uint8_t* grid = new uint8_t[block_x*block_y*3];

	for(size_t i=0;i<block_x*block_y;i++){
		grid[i*3] = 0;
		double best = 999999999999;
		for(size_t g=0;g<256;g++){
			uint16_t rsub[blockSize*blockSize];
			for(size_t y=0;y<blockSize;y++){
			for(size_t x=0;x<blockSize;x++){
				size_t location = ((y + blockSize*(i/block_x))*width + x + (i % block_x)*blockSize)*4;
				rsub[y*blockSize + x] = 256 + decoded[location] - delta(g,decoded[location + 1]);
			}
			}

			SymbolStats diffs;
			for(size_t t=0;t<256;t++){
				diffs.freqs[t] = 0;
			}
			for(size_t j=0;j<blockSize*blockSize;j++){
				if(j == 0){
					diffs.freqs[rsub[j] % 256]++;
				}
				else if(j < blockSize){
					diffs.freqs[(512 + rsub[j] - rsub[j-1]) % 256]++;
				}
				else if(j % blockSize == 0){
					diffs.freqs[(512 + rsub[j] - rsub[j-blockSize]) % 256]++;
				}
				else{
					diffs.freqs[(512 + rsub[j] - ffv1(rsub[j-1],rsub[j-blockSize],rsub[j-blockSize-1])) % 256]++;
				}
			}
			double lameSum = estimateEntropy_freq(diffs, blockSize*blockSize);
			if(lameSum < best){
				best = lameSum;
				grid[i*3] = g;
			}
		}
		grid[i*3+1] = 0;
		best = 999999999999;
		for(size_t g=0;g<256;g++){
			uint16_t bsub[blockSize*blockSize];
			for(size_t y=0;y<blockSize;y++){
			for(size_t x=0;x<blockSize;x++){
				size_t location = ((y + blockSize*(i/block_x))*width + x + (i % block_x)*blockSize)*4;
				bsub[y*blockSize + x] = 256 + decoded[location + 2] - delta(g,decoded[location + 1]);
			}
			}

			SymbolStats diffs;
			for(size_t t=0;t<256;t++){
				diffs.freqs[t] = 0;
			}
			for(size_t j=0;j<blockSize*blockSize;j++){
				if(j == 0){
					diffs.freqs[bsub[j] % 256]++;
				}
				else if(j < blockSize){
					diffs.freqs[(512 + bsub[j] - bsub[j-1]) % 256]++;
				}
				else if(j % blockSize == 0){
					diffs.freqs[(512 + bsub[j] - bsub[j-blockSize]) % 256]++;
				}
				else{
					diffs.freqs[(512 + bsub[j] - ffv1(bsub[j-1],bsub[j-blockSize],bsub[j-blockSize-1])) % 256]++;
				}
			}
			double lameSum = estimateEntropy_freq(diffs, blockSize*blockSize);
			if(lameSum < best){
				best = lameSum;
				grid[i*3 + 1] = g;
			}
		}
		grid[i*3+2] = 0;
		best = 999999999999;
		for(size_t g=0;g<256;g++){
			uint16_t bsub[blockSize*blockSize];
			for(size_t y=0;y<blockSize;y++){
			for(size_t x=0;x<blockSize;x++){
				size_t location = ((y + blockSize*(i/block_x))*width + x + (i % block_x)*blockSize)*4;
				bsub[y*blockSize + x] = 256 + decoded[location + 2] - delta(grid[i*3+1],decoded[location + 1]) - delta(g,decoded[location + 0]);
			}
			}

			SymbolStats diffs;
			for(size_t t=0;t<256;t++){
				diffs.freqs[t] = 0;
			}
			for(size_t j=0;j<blockSize*blockSize;j++){
				if(j == 0){
					diffs.freqs[bsub[j] % 256]++;
				}
				else if(j < blockSize){
					diffs.freqs[(512 + bsub[j] - bsub[j-1]) % 256]++;
				}
				else if(j % blockSize == 0){
					diffs.freqs[(512 + bsub[j] - bsub[j-blockSize]) % 256]++;
				}
				else{
					diffs.freqs[(512 + bsub[j] - ffv1(bsub[j-1],bsub[j-blockSize],bsub[j-blockSize-1])) % 256]++;
				}
			}
			double lameSum = estimateEntropy_freq(diffs, blockSize*blockSize);
			if(lameSum < best){
				best = lameSum;
				grid[i*3 + 2] = g;
			}
		}
		best = 999999999999;
		for(size_t g=0;g<256;g++){
			uint16_t bsub[blockSize*blockSize];
			for(size_t y=0;y<blockSize;y++){
			for(size_t x=0;x<blockSize;x++){
				size_t location = ((y + blockSize*(i/block_x))*width + x + (i % block_x)*blockSize)*4;
				bsub[y*blockSize + x] = 256 + decoded[location + 2] - delta(g,decoded[location + 1]) - delta(grid[i*3 + 2],decoded[location + 0]);
			}
			}

			SymbolStats diffs;
			for(size_t t=0;t<256;t++){
				diffs.freqs[t] = 0;
			}
			for(size_t j=0;j<blockSize*blockSize;j++){
				if(j == 0){
					diffs.freqs[bsub[j] % 256]++;
				}
				else if(j < blockSize){
					diffs.freqs[(512 + bsub[j] - bsub[j-1]) % 256]++;
				}
				else if(j % blockSize == 0){
					diffs.freqs[(512 + bsub[j] - bsub[j-blockSize]) % 256]++;
				}
				else{
					diffs.freqs[(512 + bsub[j] - ffv1(bsub[j-1],bsub[j-blockSize],bsub[j-blockSize-1])) % 256]++;
				}
			}
			double lameSum = estimateEntropy_freq(diffs, blockSize*blockSize);
			if(lameSum < best){
				best = lameSum;
				grid[i*3 + 1] = g;
			}
		}
		best = 999999999999;
		for(size_t g=0;g<256;g++){
			uint16_t bsub[blockSize*blockSize];
			for(size_t y=0;y<blockSize;y++){
			for(size_t x=0;x<blockSize;x++){
				size_t location = ((y + blockSize*(i/block_x))*width + x + (i % block_x)*blockSize)*4;
				bsub[y*blockSize + x] = 256 + decoded[location + 2] - delta(grid[i*3+1],decoded[location + 1]) - delta(g,decoded[location + 0]);
			}
			}

			SymbolStats diffs;
			for(size_t t=0;t<256;t++){
				diffs.freqs[t] = 0;
			}
			for(size_t j=0;j<blockSize*blockSize;j++){
				if(j == 0){
					diffs.freqs[bsub[j] % 256]++;
				}
				else if(j < blockSize){
					diffs.freqs[(512 + bsub[j] - bsub[j-1]) % 256]++;
				}
				else if(j % blockSize == 0){
					diffs.freqs[(512 + bsub[j] - bsub[j-blockSize]) % 256]++;
				}
				else{
					diffs.freqs[(512 + bsub[j] - ffv1(bsub[j-1],bsub[j-blockSize],bsub[j-blockSize-1])) % 256]++;
				}
			}
			double lameSum = estimateEntropy_freq(diffs, blockSize*blockSize);
			if(lameSum < best){
				best = lameSum;
				grid[i*3 + 2] = g;
			}
		}
	}

	std::vector<unsigned char> image;
	image.resize(block_x * block_y * 4);
	for(size_t i=0;i<block_x*block_y;i++){
		printf("%d,%d,%d\n",(int)grid[i*3+0],(int)grid[i*3+1],(int)grid[i*3+2]);
		image[i*4+0] = grid[i*3+0];
		image[i*4+1] = grid[i*3+1];
		image[i*4+2] = grid[i*3+2];
		image[i*4+3] = 255;
	}
	encodeOneStep("colour_grid.png", image, block_x, block_y);

	delete[] decoded;
	delete[] grid;

	return 0;
}
