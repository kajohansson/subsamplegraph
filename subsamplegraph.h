/*
MIT License

Copyright (c) 2017 Karl-Anders Johansson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _SUBSAMPLEGRAPH_H
#define _SUBSAMPLEGRAPH_H

#include <stdint.h>

typedef struct SSG_private SSG;

/**
 * SSG_New
 * Create a new graph object capable of receiving samples and rendering a graph as a bitmap.
 * @param pcBasefilename Fully qualified base filename including path for long term storage
 * @param bWritable Map files for writing
 * @return SSG object
 */
SSG *SSG_New(char *pcBasefilename, int bWritable);

/**
 * SSG_Teardown
 * Destructor for graph object
 * @param pThis SSG object
 */
void SSG_Teardown(SSG* pThis);

/**
 * SSG_AddValue
 * Append a sample at the end of the dataset.
 * @param pThis SSG object
 * @param fValue Value to append
 */
void SSG_AddValue(SSG *pThis, float fValue);

/**
 * SSG_Length
 * Get number of added samples, in case the caller loses track. ;)
 * @param pThis
 * @return
 */
uint64_t SSG_GetLength(SSG *pThis);

/**
 * SSG_Render
 * Renders the sample data into a receiving 8-bit luminance buffer.
 * @param pThis                    SSG object
 * @param dLeftmostPixelSamplePos  Sample pos at left edge of buffer
 * @param dRightmostPixelSamplePos Sample pos at right edge of buffer
 * @param fTopmostValue            Function value at top of buffer
 * @param fBottommostValue         Function value at bottom of buffer
 * @param pDstBuffer               Pointer to 8-bit buffer to receive pixels
 * @param iWidth                   Width of destination buffer
 * @param iHeight                  Height of destination buffer
 */
void SSG_Render(SSG* pThis, double dLeftmostPixelSamplePos, double dRightmostPixelSamplePos, float fTopmostValue, float fBottommostValue, uint8_t *pDstBuffer, int iWidth, int iHeight);

#endif //_SUBSAMPLEGRAPH_H
