#ifndef _NODE_H_
#define _NODE_H_

typedef enum
{
	NODE_UNLOADED,		// Used for nodes in unloaded blocks
	NODE_AIR,
	NODE_GRASS,
	NODE_DIRT,
	NODE_STONE,
	NODE_INVALID,		// Used for invalid nodes received from server (caused by outdated clients)
} Node;

#endif