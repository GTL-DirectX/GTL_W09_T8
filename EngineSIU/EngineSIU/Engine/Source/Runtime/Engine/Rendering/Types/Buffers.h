#pragma once

#include "HAL/PlatformType.h"

class ID3D11Buffer;

struct FVertexInfo
{
    uint32_t NumVertices;
    uint32_t Stride;
    ID3D11Buffer* VertexBuffer;
};

struct FIndexInfo
{
    uint32_t NumIndices;
    ID3D11Buffer* IndexBuffer;
};

struct FBufferInfo
{
    FVertexInfo VertexInfo;
    FIndexInfo IndexInfo;
};
