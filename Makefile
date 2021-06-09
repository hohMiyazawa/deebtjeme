LIBS=-lm -lrt

all: choh dhoh choh_colour

dhoh: dhoh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp unfilters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitreader.hpp table_decode.hpp lodepng.cpp
	g++ -o $@ $< lodepng.cpp -O3 $(LIBS)

choh: choh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitwriter.hpp table_encode.hpp entropy_estimation.hpp entropy_optimiser.hpp optimiser.hpp entropy_coding.hpp predictor_optimiser.hpp simple_encoders.hpp research_optimiser.hpp lodepng.cpp
	g++ -o $@ $< lodepng.cpp -O3 $(LIBS)

choh_colour: choh_colour.cpp lodepng.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp colour_filters.hpp varint.hpp panic.hpp table_encode.hpp
	g++ -o $@ $< lodepng.cpp -O3 $(LIBS)
