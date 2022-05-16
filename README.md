# 1 Decompress Ambarella compressed raw files

```
USAGE:
  ./amba_decompress.exe [-o FILE_OUT.RAW] [-size WIDTH HEIGHT] [-pitch PITCH] FILE_IN.RAW ...
Simplest case: try to guess parameters from file size
  ./amba_decompress.exe FILE1.RAW FILE2.RAW ...
Hard case: provide width and height and pitch (if file truncated or odd)
  ./amba_decompress.exe -size 3840 2880 -pitch 3264 FILE_IN.RAW

```

The decompression algorithm is well commented in the source file amba_decompress.c.
In short blocks of 32 pixels (12 or 10 bit per pixel) are compressed to 27 bytes.
Thus instead of 32⋅12bits=384bits algorithm uses only 27⋅8=216bits.
Compression ratio is 216/384=56.25%, e.g. instead of 12bits,
compression saves about 6.75bits. That sounds very drastic, but in
reality nearby pixels will be very similar. However transitions
between dark and bright areas will be more affected: look for
artifacts there.


It seems to me that Ambarella firmware is using the compression algorithm by default.
While my tests are limited (and only from older Xiaomi Yi and Firefly 8SE cameras), 
from memory dumps it looks to me that ordinary JPEGs in Ambarella cameras are
by default produced from internally compressed pixels. 
So the only way to be sure that in-camera JPEGs are produced from uncompressed pixels is to
force camera to dump uncompressed raw files as well (which for many cameras is hard and involves all kind of tricks).


There are sample compressed &lowast;.RAW files from Xiaomi Yi and Firefly 8SE (ff8se) for documentation purposes. 
Check [Amba_Compressed_Samples/](https://github.com/glagolj/aaraw/tree/main/Amba_Compressed_Samples)


For windows and Linux executables check: [Binary/](https://github.com/glagolj/aaraw/tree/main/Binary)




# 2 What to do with uncompressed raw files from action cameras?


Most online tutorials and tools (search "raw2dng") use wrong color calibration.
Remarkably the results are not bad, even if internal color matrix is actually from some very old camera.
There was an effort by hc1982 to provide proper color calibration for some action cameras.
I am working on another tool, so watch this space in future (and don't erase your raw files).



[Blog](https://glagolj.github.io/gg-blog/)
[Github](https://github.com/glagolj)
