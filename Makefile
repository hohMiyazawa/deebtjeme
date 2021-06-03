LIBS=-lm -lrt

all: choh dhoh

dhoh: dhoh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp unfilters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitreader.hpp table_decode.hpp
	g++ lodepng.cpp -o $@ $< -O3 $(LIBS)

choh: choh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitwriter.hpp table_encode.hpp entropy_estimation.hpp entropy_optimiser.hpp optimiser.hpp entropy_coding.hpp predictor_optimiser.hpp
	g++ lodepng.cpp -o $@ $< -O3 $(LIBS)
