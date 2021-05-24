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

Where ``speed`` is one of ``0``, ``1``, ``2``, ``3`` or ``4``, increasingly slower.

Decoding:
```
./dhoh infile.hoh outfile.png
```

Need some grayscale test images? Create them with imagemagick:
```
convert anything.png -depth 8 -colorspace Gray gray.png
```
