/*
 * misc.c <z64.me>
 *
 * this compilation unit is for building external libraries
 *
 */

#define WOW_IMPLEMENTATION
#include <wow.h>






#define STB_IMAGE_WRITE_IMPLEMENTATION
//#define STBIW_MALLOC   malloc_safe
//#define STBIW_REALLOC  realloc_safe
//#define STBIW_FREE     free
#define stbiw__fopen   wow_fopen
#define STBI_NO_BMP
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_JPEG
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG 1
#define STBI_ONLY_PNG
//#define STBI_MALLOC    malloc_safe
//#define STBI_REALLOC   realloc_safe
//#define STBI_FREE      free
#define stbi__fopen wow_fopen
#include "stb_image.h"

