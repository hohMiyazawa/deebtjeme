# Experimental lossless image compression

8bit RGB images, alpha channel ignored, input must be PNG.

Compile:
```
make
```

Usage:

```
./choh infile.png outfile.hoh speed
```

Where ``speed`` is a number in the range ``0`` to ``10`` (inclusive). increasingly slower and better compression. (Higher numbers are also accepted, but usually don't help much)

Decoding:
```
./dhoh infile.hoh outfile.png
```
