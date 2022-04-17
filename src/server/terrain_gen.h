#ifndef _TERRAIN_GEN_H_
#define _TERRAIN_GEN_H_

#include "server/server_terrain.h"
#include "terrain.h"

s32 terrain_gen_get_base_height(v2s32 pos);
void terrain_gen_chunk(TerrainChunk *chunk, List *changed_chunks); // generate a chunk (does not manage chunk state or threading)

#endif // _TERRAIN_GEN_H_
