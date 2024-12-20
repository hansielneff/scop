#ifndef OBJ_PARSER_H
#define OBJ_PARSER_H

#include "util.h"
#include "maths.h"

typedef struct
{
    Vec3 pos;
    Vec2 texCoord;
} Vertex;

typedef u32 Index;

void parseObjFile(const char *filename, Vertex** outVertices, u32* outVertexCount, Index** outIndices, u32* outIndexCount);

#endif
