# Experimental lossless image compression

8bit grayscale photographic images

Compile:
```
make
```

Usage:

```
./choh infile.png outfile.hoh speed
```

Where ``speed`` is a number in the range ``0`` to ``8`` (inclusive). increasingly slower and better compression.

There's also an experimental ``choh_colour`` version, with speeds 0-7.

Decoding:
```
./dhoh infile.hoh outfile.png
```

Need some grayscale test images? Create them with imagemagick:
```
convert anything.png -depth 8 -colorspace Gray gray.png
```
