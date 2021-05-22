LIBS=-lm -lrt

all: coder decoder tabletester testing raw_interleaving

coder: coder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp varint.hpp
	g++ -o $@ $< -O3 $(LIBS)

decoder: decoder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp unfilters.hpp varint.hpp
	g++ -o $@ $< -O3 $(LIBS)

tabletester: tabletester.cpp symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp
	g++ -o $@ $< -O3 $(LIBS)

raw_interleaving: raw_interleaving.cpp symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp
	g++ -o $@ $< -O3 $(LIBS)

testing: testing.cpp symbolstats.hpp laplace.hpp
	g++ -o $@ $< -O3 $(LIBS)
