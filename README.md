# Experimental lossless image compression

8bit photographic images

Compile:
```
make
```

Usage:

```
./choh infile.png outfile.hoh speed
```

Where ``speed`` is a number in the range ``0`` to ``8`` (inclusive). increasingly slower and better compression.

Decoding:
```
./dhoh infile.hoh outfile.png
```
