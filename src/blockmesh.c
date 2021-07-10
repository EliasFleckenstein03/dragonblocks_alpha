#include "blockmesh.h"
#include "clientnode.h"
#include "cube.h"

static v3s8 fdir[6] = {
	{+0, +0, -1},
	{+0, +0, +1},
	{-1, +0, +0},
	{+1, +0, +0},
	{+0, -1, +0},
	{+0, +1, +0},
};

static VertexBuffer make_vertices(MapBlock *block, Map *map)
{
	VertexBuffer buffer = vertexbuffer_create();

	ITERATE_MAPBLOCK {
		if (node_definitions[block->data[x][y][z].type].visible) {
			v3f offset = {x + 8.0f, y + 8.0f, z + 8.0f};

			vertexbuffer_set_texture(&buffer, client_node_definitions[block->data[x][y][z].type].texture);

			for (int f = 0; f < 6; f++) {
				v3s8 npos = {
					x + fdir[f].x,
					y + fdir[f].y,
					z + fdir[f].z,
				};

				Node neighbor;

				if (npos.x >= 0 && npos.x < 16 && npos.y >= 0 && npos.y < 16 && npos.z >= 0 && npos.z < 16)
					neighbor = block->data[npos.x][npos.y][npos.z].type;
				else
					neighbor = map_get_node(map, (v3s32) {npos.x + block->pos.x * 16, npos.y + block->pos.y * 16, npos.z + block->pos.z * 16}).type;

				if (neighbor != NODE_UNLOADED && ! node_definitions[neighbor].visible) {
					for (int v = 0; v < 6; v++) {
						Vertex vertex = cube_vertices[f][v];
						vertex.x += offset.x;
						vertex.y += offset.y;
						vertex.z += offset.z;

						vertexbuffer_add_vertex(&buffer, &vertex);
					}
				}
			}
		}
	}

	return buffer;
}

#undef GNODDEF
#undef VALIDPOS

void make_block_mesh(MapBlock *block, Map *map, Scene *scene)
{
	if (block->extra)
		((MeshObject *) block->extra)->remove = true;
	block->extra = meshobject_create(make_vertices(block, map), scene, (v3f) {block->pos.x * 16.0f - 8.0f, block->pos.y * 16.0f - 8.0f, block->pos.z * 16.0f - 8.0f});
}
