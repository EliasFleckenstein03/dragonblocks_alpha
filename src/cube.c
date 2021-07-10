#include "cube.h"

Vertex cube_vertices[6][6] = {
	{
		{-0.5, -0.5, -0.5, +0.0, +0.0},
		{+0.5, -0.5, -0.5, +1.0, +0.0},
		{+0.5, +0.5, -0.5, +1.0, +1.0},
		{+0.5, +0.5, -0.5, +1.0, +1.0},
		{-0.5, +0.5, -0.5, +0.0, +1.0},
		{-0.5, -0.5, -0.5, +0.0, +0.0},
	},
	{
		{-0.5, -0.5, +0.5, +0.0, +0.0},
		{+0.5, +0.5, +0.5, +1.0, +1.0},
		{+0.5, -0.5, +0.5, +1.0, +0.0},
		{+0.5, +0.5, +0.5, +1.0, +1.0},
		{-0.5, -0.5, +0.5, +0.0, +0.0},
		{-0.5, +0.5, +0.5, +0.0, +1.0},
	},
	{
		{-0.5, +0.5, +0.5, +1.0, +1.0},
		{-0.5, -0.5, -0.5, +0.0, +0.0},
		{-0.5, +0.5, -0.5, +0.0, +1.0},
		{-0.5, -0.5, -0.5, +0.0, +0.0},
		{-0.5, +0.5, +0.5, +1.0, +1.0},
		{-0.5, -0.5, +0.5, +1.0, +0.0},
	},
	{
		{+0.5, +0.5, +0.5, +1.0, +1.0},
		{+0.5, +0.5, -0.5, +0.0, +1.0},
		{+0.5, -0.5, -0.5, +0.0, +0.0},
		{+0.5, -0.5, -0.5, +0.0, +0.0},
		{+0.5, -0.5, +0.5, +1.0, +0.0},
		{+0.5, +0.5, +0.5, +1.0, +1.0},
	},
	{
		{-0.5, -0.5, -0.5, +0.0, +1.0},
		{+0.5, -0.5, -0.5, +1.0, +1.0},
		{+0.5, -0.5, +0.5, +1.0, +0.0},
		{+0.5, -0.5, +0.5, +1.0, +0.0},
		{-0.5, -0.5, +0.5, +0.0, +0.0},
		{-0.5, -0.5, -0.5, +0.0, +1.0},
	},
	{
		{-0.5, +0.5, -0.5, +0.0, +1.0},
		{+0.5, +0.5, -0.5, +1.0, +1.0},
		{+0.5, +0.5, +0.5, +1.0, +0.0},
		{+0.5, +0.5, +0.5, +1.0, +0.0},
		{-0.5, +0.5, +0.5, +0.0, +0.0},
		{-0.5, +0.5, -0.5, +0.0, +1.0},
	},
};

