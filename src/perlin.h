#ifndef _PERLIN_H_
#define _PERLIN_H_

#include <perlin/perlin.h> // include perlin submodule header file

typedef enum
{
	SO_NONE,
	SO_HEIGHT,
	SO_MOUNTAIN,
	SO_OCEAN,
	SO_MOUNTAIN_HEIGHT,
	SO_BOULDER_CENTER,
	SO_BOULDER,
	SO_WETNESS,
	SO_TEXTURE_OFFSET_S,
	SO_TEXTURE_OFFSET_T,
	SO_TEMPERATURE,
	SO_PINE_AREA,
	SO_PINE,
	SO_PINE_HEIGHT,
	SO_PINE_BRANCH,
} SeedOffset;

extern int seed;

#endif
