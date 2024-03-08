#pragma once

#include <cmm/cmm.h>

#include "attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct Vertex {
	Position position;
	Normal normal;
} Vertex;

typedef struct Obj {
	usize nVertices;
	usize nIndices;
	Vertex* vertices;
	u16* indices;
} Obj;

void LoadOBJ(const char* path, Obj* obj);

void FreeOBJ(Obj obj);

#ifdef __cplusplus
}
#endif