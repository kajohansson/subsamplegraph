#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <SDL.h>
#include "subsamplegraph.h"


#define WINDOW_TITLE "subsamplegraphtest"



SSG* pSSG;

float fBrownianWalk = 0.0f;
uint64_t iSamplesAdded = 0;

void addRandomSample() {
  fBrownianWalk += ((rand() & 8191)-4096) * 0.002f;
  fBrownianWalk *= (0.6+0.4*cos(iSamplesAdded*0.0003));
  SSG_AddValue(pSSG, fBrownianWalk);
  iSamplesAdded++;
}

int main(int argc, char* argv[]) {
  int ret = 0;
  int i,j,k;
  char title[255];

  pSSG = SSG_New("test", 1);

  iSamplesAdded = SSG_GetLength(pSSG);

  printf("getlength returned %ld\n", iSamplesAdded);
  if (iSamplesAdded==0) {
    printf("First time running. Generate a big chunk of randomness...\n");
    for (i = 0; i < 100000000; i++) {
      addRandomSample();
    }
  }
  unsigned int graph_width = 2048; //192;
  unsigned int graph_height = graph_width / 3;
  double dSamplesPerPixel = 200000.0 / (double)graph_width;
  float fValueAtTopmostPixel = 300.0f;
  float fValueAtBottommostPixel = -fValueAtTopmostPixel;
  float fValuesPerPixel = (fValueAtBottommostPixel - fValueAtTopmostPixel) / (float)graph_height;


  SDL_Window* window = NULL;
  SDL_Surface* surf = NULL;
  SDL_Surface* screen = NULL;
  SDL_Event event;
  SDL_Renderer* renderer = NULL;

  if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
    printf( "SDL2 could not initialize! SDL2_Error: %s\n", SDL_GetError() );
    ret = -1;
    goto ERRET;
  }

  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
    SDL_Log("SDL_GetDesktopDisplayMode failed: %s\n", SDL_GetError());
    ret = -1;
    goto ERRET;
  }

  printf("Detected screen resolution: %dx%d\n", dm.w,dm.h);
  dm.w -= 100; // Allow for some breathing room if scaling down
  dm.h -= 100;

  if (graph_width > dm.w) {
    graph_height = graph_height * dm.w/graph_width;
    graph_width = (unsigned int)dm.w;
  }
  if (graph_height > dm.h) {
    graph_width = graph_width * dm.h/graph_height;
    graph_height = (unsigned int)dm.h;
  }
  int iScale = dm.w / graph_width;
  unsigned int window_width  = graph_width  * iScale;
  unsigned int window_height = graph_height * iScale;

  printf("Using window size: %dx%d\n", window_width,window_height);
  printf("Using graph size: %dx%d\n", graph_width,graph_height);

  window = SDL_CreateWindow(
          WINDOW_TITLE,
          SDL_WINDOWPOS_CENTERED,
          SDL_WINDOWPOS_CENTERED,
          window_width,
          window_height,
          SDL_WINDOW_SHOWN);
  int x,y;
  SDL_GetWindowPosition(window, &x, &y);

  uint8_t* pGraphBuf = calloc(graph_width*graph_height, 1);
  uint32_t *framebuffer = calloc(window_width*window_height*4, 1);
  for (i=0; i<window_width*window_height; i++) framebuffer[i] = 0xff000080;

  surf = SDL_CreateRGBSurfaceFrom(framebuffer, window_width, window_height, 32, window_width*4, 0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000);



  int running = 1;
  int update = 1;
  int pause = 0;
  int clickdragging = 0;

  int iMouseX = 0;
  int iMouseY = 0;
  int iMouseWY = 0;
  double dSamplePosAtRightmostPixel = iSamplesAdded;

  int waitforautofollow = 0;
  int autofollow = 1;

  double dSamplePosAtMouseDown;
  double fValueAtMouseDown;


  while (running) {

    if (update) {

      // Zoom to fit all data
      //dSamplesPerPixel = (double)iSamplesAdded / (double)graph_width;

      // Render graph to 8-bit buffer
      SSG_Render(pSSG, dSamplePosAtRightmostPixel-graph_width*dSamplesPerPixel, dSamplePosAtRightmostPixel, fValueAtBottommostPixel-graph_height*fValuesPerPixel, fValueAtBottommostPixel, pGraphBuf, graph_width, graph_height);

      // point sample upscale to window size (to make visible pixels) and make it ARGB.
      for (x=0; x<window_width; x++) {
        for (y = 0; y < window_height; y++) {
          int gx = x / iScale;
          int gy = y / iScale;
          framebuffer[y * window_width + x] = pGraphBuf[gy * graph_width + gx] * 0x010101 + 0xff000000;
        }
      }

      screen = SDL_GetWindowSurface(window);
      SDL_BlitSurface(surf, NULL, screen, NULL);
      SDL_UpdateWindowSurface(window);

      update = 0;
    }

    int iRet = 1;
    while (iRet)
    {
      iRet = SDL_PollEvent(&event);
      switch (event.type) {
        case SDL_QUIT:
          running = 0;
          break;
        case SDL_KEYDOWN:
          if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = 0;
          if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {pause=1-pause;}
          if (event.key.keysym.scancode == SDL_SCANCODE_A) {autofollow=1-autofollow;}
          break;
        case SDL_MOUSEWHEEL:
        {
          iMouseWY = event.wheel.y;
          if (iMouseWY) {
            if (SDL_GetModState() & KMOD_ALT) {
              float fValueAtMouse = fValueAtBottommostPixel - fValuesPerPixel * (window_height - iMouseY) / iScale;
              fValuesPerPixel *= pow(2.0, -iMouseWY * 0.05);
              fValueAtBottommostPixel = fValueAtMouse + fValuesPerPixel * (window_height - iMouseY) / iScale;
            } else {
              double dSamplePosAtMouse = dSamplePosAtRightmostPixel - dSamplesPerPixel * (window_width - iMouseX) / iScale;
              dSamplesPerPixel *= pow(2.0, -iMouseWY * 0.05);
              dSamplePosAtRightmostPixel = dSamplePosAtMouse + dSamplesPerPixel * (window_width - iMouseX) / iScale;
              autofollow = 0;
            }
          }
          break;
        }
        case SDL_MOUSEMOTION:
        {
          iMouseX = event.motion.x;
          iMouseY = event.motion.y;
          if (clickdragging) {
            dSamplePosAtRightmostPixel = dSamplePosAtMouseDown + dSamplesPerPixel * (window_width - iMouseX) / iScale;
            fValueAtBottommostPixel = fValueAtMouseDown + fValuesPerPixel * (window_height - iMouseY) / iScale;

            update = 1;
          }

          break;
        }
        case SDL_MOUSEBUTTONDOWN:
          clickdragging = 1;
          autofollow = 0;
          dSamplePosAtMouseDown = dSamplePosAtRightmostPixel - dSamplesPerPixel * (window_width - iMouseX) / iScale;
          fValueAtMouseDown = fValueAtBottommostPixel - fValuesPerPixel * (window_height - iMouseY) / iScale;
          break;
        case SDL_MOUSEBUTTONUP:
          if (clickdragging) {
            clickdragging = 0;
            waitforautofollow = (dSamplePosAtRightmostPixel > iSamplesAdded) ? 1 : 0;
          }
          break;
      }
    }

    if (!pause)
      for (i=0; i<100; i++)
        addRandomSample();

    if (waitforautofollow && (iSamplesAdded>dSamplePosAtRightmostPixel) && !clickdragging) {
      waitforautofollow = 0;
      autofollow = 1;
    }
    if (autofollow) {
      dSamplePosAtRightmostPixel = iSamplesAdded;
    }
    update = 1;

    usleep(1000);
  }

  SSG_Teardown(pSSG);

  return 0;
ERRET:
  return ret;
}



