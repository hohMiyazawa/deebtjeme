LIBS=-lm -lrt

all: coder

coder: coder.cpp platform.h rans_byte.h
	g++ -o $@ $< -O3 $(LIBS)
