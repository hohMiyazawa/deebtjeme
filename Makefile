LIBS=-lm -lrt

all: choh dhoh

dhoh: dhoh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp unfilters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitreader.hpp table_decode.hpp lodepng.cpp colour_filter_utils.hpp colour_unfilters.hpp prefix_coding.hpp delta_colour.hpp
	g++ -o $@ $< lodepng.cpp -O3 $(LIBS)

choh: choh.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp varint.hpp panic.hpp numerics.hpp laplace.hpp lode_io.hpp bitwriter.hpp table_encode.hpp entropy_estimation.hpp entropy_optimiser.hpp optimiser.hpp entropy_coding.hpp predictor_optimiser.hpp simple_encoders.hpp lodepng.cpp colour_filters.hpp varint.hpp colour_filter_utils.hpp colour_simple_encoders.hpp colour_entropy_optimiser.hpp colour_optimiser.hpp colour_predictor_optimiser.hpp lz_optimiser.hpp prefix_coding.hpp delta_colour.hpp index_encoder.hpp tile_optimiser.hpp crossColour_optimiser.hpp fake_encoders.hpp
	g++ -o $@ $< lodepng.cpp -O3 $(LIBS)
