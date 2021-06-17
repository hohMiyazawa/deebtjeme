LIBS=-lm -lrt

all: layer_split high_low_split lz synth subtract ctransform row_col colour_force colour_force_grid

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

row_col: row_col.cpp ../lodepng.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

colour_force: colour_force.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)

colour_force_grid: colour_force_grid.cpp
	g++ ../lodepng.cpp -o $@ $< -O3 $(LIBS)
