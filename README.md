# Experimental lossless image compression

8bit grayscale photographic images

Usage:

```
./coder infile.raw width height outfile.hoh speed
```

Where ``infile.raw`` is raw 8bit values, and ``speed`` is one of ``0``, ``1``, ``2``, ``3`` or ``4``, increasingly slower.

You can use imagemagick to get raw 8bit values:
```
convert infile.png -depth 8 gray:infile.raw
```

Decoding:
```
./decoder infile.hoh outfile.raw
```

Imagemagick can also be used to convert raw bytes back to an image:
```
convert -size widthxheight -depth 8 gray:infile.raw outfile.png
```
