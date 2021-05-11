LIBS=-lm -lrt

all: coder decoder tabletester

coder: coder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp
	g++ -o $@ $< -O3 $(LIBS)

decoder: decoder.cpp platform.h rans_byte.h symbolstats.hpp file_io.hpp filter_utils.hpp unfilters.hpp
	g++ -o $@ $< -O3 $(LIBS)

tabletester: tabletester.cpp symbolstats.hpp file_io.hpp filter_utils.hpp filters.hpp
	g++ -o $@ $< -O3 $(LIBS)
