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
#endif //DUTILS_HEADER
