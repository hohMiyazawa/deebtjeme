LIBS=-lm -lrt

all: layer_split high_low_split lz synth subtract ctransform

layer_split: layer_split.cpp ../lode_io.hpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

high_low_split: high_low_split.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

lz: lz.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

synth: synth.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

subtract: subtract.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

ctransform: ctransform.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)
