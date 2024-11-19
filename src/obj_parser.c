#include "obj_parser.h"

#include <string.h>

void parseObjFile(const char *filename, Vertex** outVertices, u32* outVertexCount, Index** outIndices, u32* outIndexCount)
{
    ASSERT(outVertices != NULL);
    ASSERT(outVertexCount != NULL);
    ASSERT(outIndices != NULL);
    ASSERT(outIndexCount != NULL);

    FILE* file = fopen(filename, "r");
    if (file == NULL)
        PANIC("%s%s\n", "Failed to open file: ", filename);

    u32 vertexArraySize = 65536;
    Vertex* vertices = mallocOrDie(vertexArraySize * sizeof(Vertex));
    u32 vertexCount = 0;

    u32 indexArraySize = 65536;
    Index* indices = mallocOrDie(indexArraySize * sizeof(Index));
    u32 indexCount = 0;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[1023] = '\0';
        if (strncmp(line, "v ", 2) == 0) { // Vertex line
            Vertex v;
            if (sscanf(line + 2, "%f %f %f", &v.pos.x, &v.pos.y, &v.pos.z) == 3) {
                if (vertexCount < vertexArraySize) {
                    vertices[vertexCount++] = v;
                } else {
                    PANIC("%s\n", "Too many vertices");
                }
            }
        } else if (strncmp(line, "f ", 2) == 0) { // Face line
            int idx[32];
            u32 indexInFaceCount = sscanf(line + 2,
                "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                idx, idx+1, idx+2, idx+3, idx+4, idx+5, idx+6, idx+7, idx+8, idx+9, idx+10, idx+11, idx+12, idx+13, idx+14, idx+15,
                idx+16, idx+17, idx+18, idx+19, idx+20, idx+21, idx+22, idx+23, idx+24, idx+25, idx+26, idx+27, idx+28, idx+29, idx+30, idx+31);
            
            if (indexInFaceCount < 3)
                PANIC("%s\n", "Invalid face in obj file");

            for (u32 i = 2; i < indexInFaceCount; i++)
            {
                if (indexCount < indexArraySize)
                {
                    indices[indexCount++] = idx[0] - 1;
                    indices[indexCount++] = idx[i - 1] - 1;
                    indices[indexCount++] = idx[i] - 1;
                }
                else
                {
                    PANIC("%s\n", "Too many indices");
                }
            }
        }
    }

    *outVertexCount = vertexCount;
    *outIndexCount = indexCount;
    *outVertices = mallocOrDie(vertexCount * sizeof(Vertex));
    *outIndices = mallocOrDie(indexCount * sizeof(Index));
    memcpy(*outVertices, vertices, vertexCount * sizeof(Vertex));
    memcpy(*outIndices, indices, indexCount * sizeof(Index));
    freeAndNull(vertices);
    freeAndNull(indices);
}
