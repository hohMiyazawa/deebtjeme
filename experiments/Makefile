LIBS=-lm -lrt

all: layer_split high_low_split

layer_split: layer_split.cpp ../lode_io.hpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

high_low_split: high_low_split.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)
