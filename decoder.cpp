#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "rans_byte.h"

// This is just the sample program. All the meat is in rans_byte.h.

static void panic(const char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    fputs("Error: ", stderr);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
    fputs("\n", stderr);

    exit(1);
}

static uint8_t* read_file(char const* filename, size_t* out_size)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
        panic("file not found: %s\n", filename);

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* buf = new uint8_t[size];
    if (fread(buf, size, 1, f) != 1)
        panic("read failed\n");

    fclose(f);
    if (out_size)
        *out_size = size;

    return buf;
}

uint32_t readVarint(uint8_t* pointer){
	uint32_t value = *pointer;
	++pointer;
	if(value >> 7){
		value = (value % (1<<7)) << 7;
		uint32_t value2 = *pointer;
		++pointer;
		value += value2;
	}
	return value;
}

int main()
{
	size_t in_size;
	uint8_t* in_bytes = read_file("book1", &in_size);

	uint8_t* fileIndex = in_bytes;
	uint32_t width =    readVarint(fileIndex) + 1;
	uint32_t heigth =   readVarint(fileIndex) + 1;
	
	delete[] in_bytes;
	return 0;
}
