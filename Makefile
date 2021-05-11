LIBS=-lm -lrt

all: coder decoder

coder: coder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp
	g++ -o $@ $< -O3 $(LIBS)

decoder: decoder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp
	g++ -o $@ $< -O3 $(LIBS)
