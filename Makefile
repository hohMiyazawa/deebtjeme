LIBS=-lm -lrt

all: main coder

main: main.cpp platform.h rans_byte.h
	g++ -o $@ $< -O3 $(LIBS)

coder: coder.cpp platform.h rans_byte.h
	g++ -o $@ $< -O3 $(LIBS)
