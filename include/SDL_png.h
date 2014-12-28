#ifndef _SDL_png_h
#define _SDL_png_h

#include <SDL_surface.h>
#include <SDL_rwops.h>

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface *SDL_LoadPNG_RW(SDL_RWops *src, int freesrc);
SDL_Surface *SDL_LoadPNG(const char *file);

#ifdef __cplusplus
}
#endif

#endif
