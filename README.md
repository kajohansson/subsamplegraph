About
==============
Render a collection of samples as a waveform with proper subsampling and interpolation of the data

 * Handles reasonably large datasets without slowdown.
 * In-mem for now, and relies on paging when running out of main memory.
 * 


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
