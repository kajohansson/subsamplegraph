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

#include <malloc.h>
#include <math.h>
#include "subsamplegraph.h"

#define MAX_MIP_LOD 24

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
  float *pfBuffer;
  int iWritePointer;
  int iAllocSize;
} Buffer;

typedef struct {
  Buffer minBuffer;
  Buffer maxBuffer;
} LodBuffer;

struct SSG_private {
  Buffer samples;
  LodBuffer aLodBuffers[MAX_MIP_LOD];
  uint8_t aiGammaxlat[256];
};

SSG *SSG_New() {
  int i;
  SSG *pThis = calloc(sizeof(SSG), 1);
  for (i=0; i<256; i++) {
    pThis->aiGammaxlat[i] = (uint8_t)sqrt(256*i);
  }
  return pThis;
}

void SSG_Teardown(SSG* pThis) {
  // TODO: Free sample and lodbuffers here
}


static void addSample(SSG* pThis, float fSample, int iMode, int iLevel) {
  Buffer* pBuffer = NULL;
  if (iMode==0) {
    pBuffer = &pThis->samples;
  } else {
    pBuffer = iMode>0 ? &pThis->aLodBuffers[iLevel-1].maxBuffer : &pThis->aLodBuffers[iLevel-1].minBuffer;
  }

  if (pBuffer->iWritePointer >= pBuffer->iAllocSize) {
    pBuffer->iAllocSize = (pBuffer->iAllocSize == 0) ? 1024 : (pBuffer->iAllocSize*2);
    pBuffer->pfBuffer = realloc(pBuffer->pfBuffer, pBuffer->iAllocSize*sizeof(float));
  }
  pBuffer->pfBuffer[pBuffer->iWritePointer] = fSample;
  pBuffer->iWritePointer++;

  if (iLevel<MAX_MIP_LOD) {
    if ((pBuffer->iWritePointer & 1) == 0) {
      float fSampleA = pBuffer->pfBuffer[pBuffer->iWritePointer - 2];
      float fSampleB = pBuffer->pfBuffer[pBuffer->iWritePointer - 1];
      if (iMode==0) {
        addSample(pThis, MIN(fSampleA,fSampleB), -1, iLevel+1);
        addSample(pThis, MAX(fSampleA,fSampleB),  1, iLevel+1);
      } else if (iMode==-1) {
        addSample(pThis, MIN(fSampleA,fSampleB), -1, iLevel+1);
      } else {
        addSample(pThis, MAX(fSampleA,fSampleB),  1, iLevel+1);
      }
    }
  }
}
static void getSample(SSG* pThis, int iSamplePos, int iLevel, float* pfOutMin, float* pfOutMax) {
  *pfOutMin = 0.0f;
  *pfOutMax = 0.0f;
  if (iLevel >= MAX_MIP_LOD) return;

  Buffer *pMinB = (iLevel == 0) ? &pThis->samples : &pThis->aLodBuffers[iLevel-1].minBuffer;
  Buffer *pMaxB = (iLevel == 0) ? &pThis->samples : &pThis->aLodBuffers[iLevel-1].maxBuffer;

  if (iSamplePos>=0 && iSamplePos<pMinB->iWritePointer) *pfOutMin = pMinB->pfBuffer[iSamplePos];
  if (iSamplePos>=0 && iSamplePos<pMaxB->iWritePointer) *pfOutMax = pMaxB->pfBuffer[iSamplePos];
}
static int linear(float v1, float v2, float f) {
  int iv = 255.0f * (v1+(v2-v1)*f);
  return iv;
}



void SSG_AddValue(SSG *pThis, float fValue) {
  addSample(pThis, fValue, 0,0);
}
int SSG_GetLength(SSG *pThis) {
  return pThis->samples.iWritePointer;
}

void SSG_Render(SSG* pThis, double dLeftmostPixelSamplePos, double dRightmostPixelSamplePos, float fTopmostValue, float fBottommostValue, uint8_t *pDstBuffer, int iWidth, int iHeight) {


  double dSamplesPerPixel = (dRightmostPixelSamplePos - dLeftmostPixelSamplePos) / (double)iWidth;

  float fUnitsPerPixel = (fTopmostValue - fBottommostValue) / (float)iHeight;
  float fZeroAtYPixel = (float)iHeight * fTopmostValue / (fTopmostValue - fBottommostValue);

  int x,y,i;
  float fLOD = (float)(log(dSamplesPerPixel) / log(2.0) - 0.0);
  if (fLOD<0.0f) fLOD=0.0f;
  int iLOD = (int)fLOD;
  float fFracLod = fLOD - iLOD;
  float fLodScale = (1.0f/(1<<iLOD));

  float fPixelsPerUnit = 1.0f / fUnitsPerPixel;
  double fSamplePos = dRightmostPixelSamplePos - iWidth * dSamplesPerPixel;

  for (x=0; x<iWidth; x++) {
    double fBaseLodSamplePos = fSamplePos * fLodScale;
    double fNextLodSamplePos = fBaseLodSamplePos * 0.5 - 0.5;

    int iBaseLodSamplePos = (int)fBaseLodSamplePos;
    int iNextLodSamplePos = (int)fNextLodSamplePos;
    float fBaseLodSamplePosFrac = fBaseLodSamplePos - iBaseLodSamplePos;
    float fNextLodSamplePosFrac = fNextLodSamplePos - iNextLodSamplePos;

    if (dSamplesPerPixel < 1.0) {
      // TODO: Make it filtered? Quite jittery now...
      float v1, v2;
      getSample(pThis, fSamplePos - dSamplesPerPixel, 0, &v1, &v1); v1 = fZeroAtYPixel - fPixelsPerUnit * v1;
      getSample(pThis, fSamplePos                   , 0, &v2, &v2); v2 = fZeroAtYPixel - fPixelsPerUnit * v2;
      int miny = (int)MIN(v1,v2);
      int maxy = (int)MAX(v1,v2);
      for (y=0; y<iHeight; y++) {
        pDstBuffer[y*iWidth + x] = (miny<=y && y<=maxy) ? 255 : 0;
      }
    } else {
      float afBaseMin[3], afBaseMax[3];
      float afNextMin[3], afNextMax[3];
      for (i = 0; i < 3; i++) {
        getSample(pThis, iBaseLodSamplePos + i, iLOD    , &afBaseMin[i], &afBaseMax[i]);
        getSample(pThis, iNextLodSamplePos + i, iLOD + 1, &afNextMin[i], &afNextMax[i]);
        afBaseMin[i] = fZeroAtYPixel - fPixelsPerUnit * afBaseMin[i];
        afBaseMax[i] = fZeroAtYPixel - fPixelsPerUnit * afBaseMax[i];
        afNextMin[i] = fZeroAtYPixel - fPixelsPerUnit * afNextMin[i];
        afNextMax[i] = fZeroAtYPixel - fPixelsPerUnit * afNextMax[i];
      }
      for (y = 0; y < iHeight; y++) {
        int v02 = ((((int) MIN(afBaseMax[0],afBaseMin[1])) <= y) && (y <= ((int) MAX(afBaseMin[0],afBaseMax[1])))) ? 1 : 0;
        int v03 = ((((int) MIN(afBaseMax[1],afBaseMin[2])) <= y) && (y <= ((int) MAX(afBaseMin[1],afBaseMax[2])))) ? 1 : 0;
        int v12 = ((((int) MIN(afNextMax[0],afNextMin[1])) <= y) && (y <= ((int) MAX(afNextMin[0],afNextMax[1])))) ? 1 : 0;
        int v13 = ((((int) MIN(afNextMax[1],afNextMin[2])) <= y) && (y <= ((int) MAX(afNextMin[1],afNextMax[2])))) ? 1 : 0;

        int iv0 = linear(v02, v03, fBaseLodSamplePosFrac);
        int iv1 = linear(v12, v13, fNextLodSamplePosFrac);

        uint8_t iv = (uint8_t)(iv0 + (iv1 - iv0) * fFracLod); // TODO: Bad with float->int->float->int.
        iv = pThis->aiGammaxlat[iv];

        pDstBuffer[y * iWidth + x] = iv;
      }
    }
    fSamplePos += dSamplesPerPixel;
  }
}
