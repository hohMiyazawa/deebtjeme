#ifndef FILE_IO_HEADER
#define FILE_IO_HEADER

#include <fstream>
#include <stdarg.h>

static void panic(const char *fmt, ...){
	va_list arg;

	va_start(arg, fmt);
	fputs("Error: ", stderr);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	fputs("\n", stderr);

	exit(1);
}

static void write_file(char const* filename, uint8_t* start, size_t size){
	std::ofstream file(filename,std::ofstream::binary);
	file.write((char*)start, size);
	file.close();
}

static uint8_t* read_file(char const* filename, size_t* out_size){
	FILE* f = fopen(filename, "rb");
	if (!f){
		panic("file not found: %s\n", filename);
	}

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t* buf = new uint8_t[size];
	if (fread(buf, size, 1, f) != 1){
		panic("read failed\n");
	}

	fclose(f);
	if (out_size){
		*out_size = size;
	}

	return buf;
}

#endif // FILE_IO_HEADER
