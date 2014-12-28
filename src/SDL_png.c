#include <stdlib.h>
#include "SDL_png.h"
#include <png.h>

static void png_read_data(png_structp ctx, png_bytep area, png_size_t size) {
  SDL_RWops *src;
  src = (SDL_RWops *)png_get_io_ptr(ctx);
  SDL_RWread(src, area, size, 1);
}

SDL_Surface *SDL_LoadPNG(const char* file) {
  if (file == NULL) return NULL;
  SDL_RWops *fileObj = SDL_RWFromFile(file, "rb");
  if (!file) {
    SDL_SetError("LOAD PNG: File not exists");
    return NULL;
  }
  return SDL_LoadPNG_RW(fileObj, 1);
}

SDL_Surface *SDL_LoadPNG_RW(SDL_RWops *src, int freesrc) {
  Sint64 start = 0;
  const char *error = NULL;
  SDL_Surface *volatile surface = NULL;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_uint_32 width = 0, height = 0;
  int bit_depth, color_type, interlace_type, num_channels;
  Uint32 Rmask;
  Uint32 Gmask;
  Uint32 Bmask;
  Uint32 Amask;
  SDL_Palette *palette = NULL;
  png_bytep *volatile row_pointers = NULL;
  int row, i;
  int ckey = -1;
  png_color_16 *transv = NULL;
  png_byte buffer[8];

  if (!src) {
    return NULL;
  }
  start = SDL_RWtell(src);

  if (SDL_RWread(src, buffer, 8, 1) != 1) {
    error = "LOAD PNG: can't read file";
    goto done;
  }
  if (png_sig_cmp(buffer, 0, 8)) {
    error = "LOAD PNG: not really a png";
    goto done;
  }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                   NULL,NULL,NULL);
  if (png_ptr == NULL) {
    error = "LOAD PNG: Internal PNG create read struct failure";
    goto done;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    error = "LOAD PNG: Internal PNG create info struct failure";
    goto done;
  }

  if (setjmp(*png_set_longjmp_fn(png_ptr, longjmp, sizeof (jmp_buf)))) {
    error = "LOAD PNG: Internal PNG set longjmp failure";
    goto done;
  }
    
  png_set_read_fn(png_ptr, src, png_read_data);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
               &color_type, &interlace_type, NULL, NULL);

  png_set_strip_16(png_ptr) ;

  png_set_packing(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY)
    png_set_expand(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    int num_trans;
    Uint8 *trans;
    png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &transv);
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
      int j, t = -1;
      for (j = 0; j < num_trans; j++) {
        if (trans[j] == 0) {
          if (t >= 0) {
            break;
          }
          t = j;
        } else if (trans[j] != 255) {
          break;
        }
      }
      if (j == num_trans) {
        ckey = t;
      } else {
        png_set_expand(png_ptr);
      }
    } else {
      ckey = 0;
    }
  }

  if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
               &color_type, &interlace_type, NULL, NULL);

  Rmask = Gmask = Bmask = Amask = 0 ;
  num_channels = png_get_channels(png_ptr, info_ptr);
  if (num_channels >= 3) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    Rmask = 0x000000FF;
    Gmask = 0x0000FF00;
    Bmask = 0x00FF0000;
    Amask = (num_channels == 4) ? 0xFF000000 : 0;
#else
    int s = (num_channels == 4) ? 0 : 8;
    Rmask = 0xFF000000 >> s;
    Gmask = 0x00FF0000 >> s;
    Bmask = 0x0000FF00 >> s;
    Amask = 0x000000FF >> s;
#endif
  }
  surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
                                 bit_depth*num_channels,
                                 Rmask,Gmask,Bmask,Amask);
  if (surface == NULL) {
    error = SDL_GetError();
    goto done;
  }

  if (ckey != -1) {
    if (color_type != PNG_COLOR_TYPE_PALETTE) {
      ckey = SDL_MapRGB(surface->format,
                        (Uint8)transv->red,
                        (Uint8)transv->green,
                        (Uint8)transv->blue);
    }
    SDL_SetColorKey(surface, SDL_TRUE, ckey);
  }

  row_pointers = (png_bytep*)SDL_malloc(sizeof(png_bytep)*height);
  if (row_pointers == NULL) {
    error = "LOAD PNG: Out of memory";
    goto done;
  }
  for (row = 0; row < (int)height; row++) {
    row_pointers[row] = (png_bytep)
      (Uint8 *)surface->pixels + row*surface->pitch;
  }

  png_read_image(png_ptr, row_pointers);

  palette = surface->format->palette;
  if (palette) {
    int png_num_palette;
    png_colorp png_palette;
    png_get_PLTE(png_ptr, info_ptr, &png_palette, &png_num_palette);
    if (color_type == PNG_COLOR_TYPE_GRAY) {
      palette->ncolors = 256;
      for (i = 0; i < 256; i++) {
        palette->colors[i].r = i;
        palette->colors[i].g = i;
        palette->colors[i].b = i;
      }
    } else if (png_num_palette > 0 ) {
      palette->ncolors = png_num_palette;
      for ( i=0; i<png_num_palette; ++i ) {
        palette->colors[i].b = png_palette[i].blue;
        palette->colors[i].g = png_palette[i].green;
        palette->colors[i].r = png_palette[i].red;
      }
    }
  }

 done:
  if (png_ptr) {
    png_destroy_read_struct(&png_ptr,
                            info_ptr ? &info_ptr : (png_infopp)0,
                            (png_infopp)0);
  }
  if (row_pointers) {
    SDL_free(row_pointers);
  }
  if (error) {
    SDL_RWseek(src, start, RW_SEEK_SET);
    if (surface) {
      SDL_FreeSurface(surface);
      surface = NULL;
    }
    SDL_SetError(error);
  }
  if (src && freesrc) {
    SDL_RWclose(src);
  }
  return surface;
}
