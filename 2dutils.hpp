#ifndef DUTILS_HEADER
#define DUTILS_HEADER

size_t tileIndexFromPixel(
	size_t pixel,
	uint32_t width,
	uint32_t b_width,
	size_t blockSize_w,
	size_t blockSize_h
){
	uint32_t y = pixel / width;
	uint32_t x = pixel % width;
	uint32_t b_y = y / blockSize_h;
	uint32_t b_x = x / blockSize_w;
	return b_y * b_width + b_x;
}

uint8_t* bitmap_expander(uint8_t* bitmap,uint32_t width,uint32_t height){
	uint8_t* bitmap_expanded = new uint8_t[width*height*4];
	for(size_t i=0;i<width*height;i++){
		bitmap_expanded[i*4 + 0] = bitmap[i];
		bitmap_expanded[i*4 + 1] = bitmap[i];
		bitmap_expanded[i*4 + 2] = bitmap[i];
		bitmap_expanded[i*4 + 3] = 255;
	}
	return bitmap_expanded;
}

uint8_t* alpha_expander(uint8_t* bitmap,uint32_t width,uint32_t height){
	uint8_t* bitmap_expanded = new uint8_t[width*height*4];
	for(size_t i=0;i<width*height;i++){
		//note: switches GRB to RGB
		bitmap_expanded[i*4 + 0] = bitmap[i*3 + 1];
		bitmap_expanded[i*4 + 1] = bitmap[i*3 + 0];
		bitmap_expanded[i*4 + 2] = bitmap[i*3 + 2];
		bitmap_expanded[i*4 + 3] = 255;
	}
	return bitmap_expanded;
}

bool greyscale_test(uint8_t* RGBA,uint32_t width,uint32_t height){
	for(size_t i=0;i<width*height;i+=4){
		if(
			RGBA[i] != RGBA[i+1]
			|| RGBA[i] != RGBA[i+2]
		){
			return false;
		}
	}
	return true;
}

#endif //DUTILS_HEADER
