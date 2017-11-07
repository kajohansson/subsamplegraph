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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "subsamplegraph.h"

#define BLOCK_SIZE (2*1024*1024/sizeof(float))

typedef struct {
  int fd;                 // file mapped to memory block
  int bWritable;          // is memory mapped as writable and resizable?
  float *pfBuffer;        // memory mapped file as floats
  uint64_t iAllocated;    // number of floats allocated
  uint64_t iWritePointer; // index of next element to write
#ifdef DEBUGPRINTS
  char *pFilenamecopy;
#endif
} Buffer;

int mapfiletomemblock(char *pFilename, Buffer* pOutMemblock, int bWritable) {
#ifdef DEBUGPRINTS
  pOutMemblock->pFilenamecopy = malloc(strlen(pFilename)+1);
  strcpy(pOutMemblock->pFilenamecopy, pFilename);
#endif
  int fd = open(pFilename,O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  uint64_t iUsed = lseek(fd, 0, SEEK_END) / sizeof(float); // Number of elements in file

  uint64_t iAllocated = iUsed;
  if (bWritable) {
    iAllocated = (iUsed + BLOCK_SIZE - 1) & (~(BLOCK_SIZE-1));
    if (iAllocated==0) iAllocated = BLOCK_SIZE; // If all new file, give it some initial space

    ftruncate(fd, iAllocated*sizeof(float));
  }

  pOutMemblock->pfBuffer = mmap (0, iAllocated*sizeof(float), PROT_READ | (bWritable?PROT_WRITE:0), MAP_SHARED, fd, 0);
  // TODO: Handle error
  if (pOutMemblock->pfBuffer == MAP_FAILED) printf("%d: %s\n", errno, strerror(errno));

  pOutMemblock->iWritePointer = iUsed;
  pOutMemblock->iAllocated = iAllocated;
  pOutMemblock->fd = fd;
  pOutMemblock->bWritable = bWritable;
  return 0;
}


int updatesize(Buffer* pMb, uint64_t iSize) {
#ifdef DEBUGPRINTS
  printf("updatesize(%s, %ld) - last alloced %ld ", pMb->pFilenamecopy, iSize, pMb->iAllocated);
#endif

  if (!pMb->bWritable) return -1;
  if (iSize <= pMb->iAllocated) return 0;

  uint64_t iNewTotalAlloc = (iSize + BLOCK_SIZE - 1) & (~(BLOCK_SIZE-1));

#ifdef DEBUGPRINTS
  printf("newtotalalloc %ld \n", iNewTotalAlloc);
#endif

  ftruncate(pMb->fd, iNewTotalAlloc*sizeof(float));

  uint64_t iNewExtraAlloc = iNewTotalAlloc - pMb->iAllocated;

#ifdef EXPERIMENTAL_TAIL_ALLOC
  float* pfTail = mmap (pMb->pfBuffer + pMb->iAllocated, iNewExtraAlloc*sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED, pMb->fd, pMb->iAllocated * sizeof(float));
  // TODO: Handle error
  if (pfTail == MAP_FAILED) printf("%d: %s\n", errno, strerror(errno));


  if (pfTail != (pMb->pfBuffer + pMb->iAllocated)) {
    munmap(pfTail, iNewExtraAlloc*sizeof(float));
#endif

    // Release mapping and create a new one.
    msync(pMb->pfBuffer, pMb->iAllocated*sizeof(float), MS_SYNC);
    munmap(pMb->pfBuffer, pMb->iAllocated*sizeof(float));
    pMb->pfBuffer = mmap (0, iNewTotalAlloc*sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED, pMb->fd, 0);
    if (pMb->pfBuffer == MAP_FAILED) printf("%d: %s\n", errno, strerror(errno));
    // TODO: Handle error
#ifdef EXPERIMENTAL_TAIL_ALLOC
  }
#endif

  pMb->iAllocated = iNewTotalAlloc;
  return 0;
}

int releasememblock(Buffer *pB) {

  if (pB->bWritable) {
    // truncate file to only used part.
    ftruncate(pB->fd, pB->iWritePointer * sizeof(float));
  }

  msync(pB->pfBuffer, pB->iAllocated*sizeof(float), MS_SYNC);
  munmap(pB->pfBuffer, pB->iAllocated*sizeof(float));

  close(pB->fd);

  pB->fd = 0;
  pB->pfBuffer = NULL;
  pB->iAllocated = 0;
  pB->iWritePointer = 0;
}

#define MAX_MIP_LOD 24

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


typedef struct {
  Buffer minBuffer;
  Buffer maxBuffer;
} LodBuffer;

struct SSG_private {
  Buffer samples;
  LodBuffer aLodBuffers[MAX_MIP_LOD];
  uint8_t aiGammaxlat[256];
  int bWritable;
};

SSG *SSG_New(char *pcBasefilename, int bWritable) {
  int i;
  SSG *pThis = calloc(sizeof(SSG), 1);
  for (i=0; i<256; i++) {
    pThis->aiGammaxlat[i] = (uint8_t)sqrt(256*i);
  }
  char acFilename[1024];
  sprintf(acFilename, "%s_rawsamples.bin", pcBasefilename);
  mapfiletomemblock(acFilename, &pThis->samples, bWritable);
  for (i=0; i<MAX_MIP_LOD; i++) {
    sprintf(acFilename, "%s_LOD_%d_MIN.bin", pcBasefilename, i);
    mapfiletomemblock(acFilename, &pThis->aLodBuffers[i].minBuffer, bWritable);
    sprintf(acFilename, "%s_LOD_%d_MAX.bin", pcBasefilename, i);
    mapfiletomemblock(acFilename, &pThis->aLodBuffers[i].maxBuffer, bWritable);
  }

  return pThis;
}

void SSG_Teardown(SSG* pThis) {
  int i;

  releasememblock(&pThis->samples);
  for (i=0; i<MAX_MIP_LOD; i++) {
    releasememblock(&pThis->aLodBuffers[i].minBuffer);
    releasememblock(&pThis->aLodBuffers[i].maxBuffer);
  }
}


static void addSample(SSG* pThis, float fSample, int iMode, int iLevel) {
  Buffer* pBuffer = NULL;
  if (iMode==0) {
    pBuffer = &pThis->samples;
  } else {
    pBuffer = iMode>0 ? &pThis->aLodBuffers[iLevel-1].maxBuffer : &pThis->aLodBuffers[iLevel-1].minBuffer;
  }

  if (pBuffer->iWritePointer >= pBuffer->iAllocated) {
    updatesize(pBuffer, pBuffer->iWritePointer+1);
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
static void getSample(SSG* pThis, int64_t iSamplePos, int iLevel, float* pfOutMin, float* pfOutMax) {
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
uint64_t SSG_GetLength(SSG *pThis) {
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
  double dSamplePos = dLeftmostPixelSamplePos;

  for (x=0; x<iWidth; x++) {
    double dBaseLodSamplePos = dSamplePos * fLodScale;
    double dNextLodSamplePos = dBaseLodSamplePos * 0.5 - 0.5;

    int iBaseLodSamplePos = (int)dBaseLodSamplePos;
    int iNextLodSamplePos = (int)dNextLodSamplePos;
    float fBaseLodSamplePosFrac = (float)(dBaseLodSamplePos - iBaseLodSamplePos);
    float fNextLodSamplePosFrac = (float)(dNextLodSamplePos - iNextLodSamplePos);

    if (dSamplesPerPixel < 1.0) {
      // TODO: Make it filtered? Quite jittery now...
      float v1, v2;
      getSample(pThis, (int64_t)(dSamplePos - dSamplesPerPixel), 0, &v1, &v1); v1 = fZeroAtYPixel - fPixelsPerUnit * v1;
      getSample(pThis, (int64_t) dSamplePos                    , 0, &v2, &v2); v2 = fZeroAtYPixel - fPixelsPerUnit * v2;
      int miny = (int)MIN(v1,v2);
      int maxy = (int)MAX(v1,v2);
      for (y=0; y<iHeight; y++) {
        pDstBuffer[y*iWidth + x] = (uint8_t)((miny<=y && y<=maxy) ? 255 : 0);
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
    dSamplePos += dSamplesPerPixel;
  }
}
