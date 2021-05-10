LIBS=-lm -lrt

all: coder decoder

coder: coder.cpp platform.h rans_byte.h
	g++ -o $@ $< -O3 $(LIBS)

decoder: decoder.cpp platform.h rans_byte.h
	g++ -o $@ $< -O3 $(LIBS)
