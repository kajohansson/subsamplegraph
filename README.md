About
==============
Render a collection of samples as a waveform with proper subsampling and interpolation of the data

 * Handles large datasets without slowdown.
 * Uses file backing for all memory buffers, mmap:ed into memory.

Todo:s
======

Rendering optimizations
-----------------------
 * Cache computations between render calls. Today it's completely stateless.
 * Faster linear filtering without superflous type conversions (float->int->float->int)
 * ... Lots more. :)

API additions
-------------
 * Getters for unfiltered and subsampled data - alleviate the need for caller to keep separate copy of data
 * ...
