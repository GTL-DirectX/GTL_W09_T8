
#include "ShaderRegisters.hlsl"

static const float PI = 3.141592;
static const int BONE_GIZMO_LINES_PER_CIRCLE = 12; 
static const int BONE_GIZMO_CIRCLES_PER_SPHERE = 3;
static const int BONE_GIZMO_TOTAL_LINES_PER_BONE = BONE_GIZMO_LINES_PER_CIRCLE * BONE_GIZMO_CIRCLES_PER_SPHERE;

cbuffer GridParametersData : register(b1)
{
    float GridSpacing;
    int GridCount; // 총 grid 라인 수
    float2 Padding1;
    
    float3 GridOrigin; // Grid의 중심
    float Padding;
};

cbuffer PrimitiveCounts : register(b3)
{
    int BoundingBoxCount; // 렌더링할 AABB의 개수
    int ConeCount; // 렌더링할 cone의 개수
    int BoneCount; // Begin Test
    int pad;
};

struct FBoundingBoxData
{
    float3 bbMin;
    float padding0;
    float3 bbMax;
    float padding1;
};
struct FConeData
{
    float3 ConeApex; // 원뿔의 꼭짓점
    float ConeRadius; // 원뿔 밑면 반지름
    
    float3 ConeBaseCenter; // 원뿔 밑면 중심
    float ConeHeight; // 원뿔 높이 (Apex와 BaseCenter 간 차이)
    float4 Color;
    
    int ConeSegmentCount; // 원뿔 밑면 분할 수
    float pad[3];
};

struct FOrientedBoxCornerData
{
    float4 corners[8]; // 회전/이동 된 월드 공간상의 8꼭짓점
};

struct FBoneData
{
    float3 Center;
    float PadBoneGizmo01;

    //float3 Apex;
    //float PadBoneGizmo02;

    float4 Color;
};
    
StructuredBuffer<FBoundingBoxData> g_BoundingBoxes : register(t2);
StructuredBuffer<FConeData> g_ConeData : register(t3);
StructuredBuffer<FOrientedBoxCornerData> g_OrientedBoxes : register(t4);
StructuredBuffer<FBoneData> g_BoneData : register(t5);
static const int BB_EdgeIndices[12][2] =
{
    { 0, 1 },
    { 1, 3 },
    { 3, 2 },
    { 2, 0 }, // 앞면
    { 4, 5 },
    { 5, 7 },
    { 7, 6 },
    { 6, 4 }, // 뒷면
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 } // 측면
};

struct VS_INPUT
{
    uint vertexID : SV_VertexID; // 0 또는 1: 각 라인의 시작과 끝
    uint instanceID : SV_InstanceID; // 인스턴스 ID로 grid, axis, bounding box를 구분
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 WorldPosition : POSITION;
    float4 Color : COLOR;
    uint instanceID : SV_InstanceID;
};

/////////////////////////////////////////////////////////////////////////
// Grid 위치 계산 함수
/////////////////////////////////////////////////////////////////////////
float3 ComputeGridPosition(uint instanceID, uint vertexID)
{
    int halfCount = GridCount / 2;
    float centerOffset = halfCount * 0.5; // grid 중심이 원점에 오도록

    float3 startPos;
    float3 endPos;
    
    if (instanceID < halfCount)
    {
        // 수직선: X 좌표 변화, Y는 -centerOffset ~ +centerOffset
        float x = GridOrigin.x + (instanceID - centerOffset) * GridSpacing;
        if (abs(x - GridOrigin.x) < 0.001)
        {
            startPos = float3(0, 0, 0);
            endPos = float3(0, (GridOrigin.y - centerOffset * GridSpacing), 0);
        }
        else
        {
            startPos = float3(x, GridOrigin.y - centerOffset * GridSpacing, GridOrigin.z);
            endPos = float3(x, GridOrigin.y + centerOffset * GridSpacing, GridOrigin.z);
        }
    }
    else
    {
        // 수평선: Y 좌표 변화, X는 -centerOffset ~ +centerOffset
        int idx = instanceID - halfCount;
        float y = GridOrigin.y + (idx - centerOffset) * GridSpacing;
        if (abs(y - GridOrigin.y) < 0.001)
        {
            startPos = float3(0, 0, 0);
            endPos = float3(-(GridOrigin.x + centerOffset * GridSpacing), 0, 0);
        }
        else
        {
            startPos = float3(GridOrigin.x - centerOffset * GridSpacing, y, GridOrigin.z);
            endPos = float3(GridOrigin.x + centerOffset * GridSpacing, y, GridOrigin.z);
        }

    }
    return (vertexID == 0) ? startPos : endPos;
}

/////////////////////////////////////////////////////////////////////////
// Axis 위치 계산 함수 (총 3개: X, Y, Z)
/////////////////////////////////////////////////////////////////////////
float3 ComputeAxisPosition(uint axisInstanceID, uint vertexID)
{
    float3 start = float3(0.0, 0.0, 0.0);
    float3 end;
    float zOffset = 0.f;
    if (axisInstanceID == 0)
    {
        // X 축: 빨간색
        end = float3(1000000.0, 0.0, zOffset);
    }
    else if (axisInstanceID == 1)
    {
        // Y 축: 초록색
        end = float3(0.0, 1000000.0, zOffset);
    }
    else if (axisInstanceID == 2)
    {
        // Z 축: 파란색
        end = float3(0.0, 0.0, 1000000.0 + zOffset);
    }
    else
    {
        end = start;
    }
    return (vertexID == 0) ? start : end;
}

/////////////////////////////////////////////////////////////////////////
// Bounding Box 위치 계산 함수
// bbInstanceID: StructuredBuffer에서 몇 번째 bounding box인지
// edgeIndex: 해당 bounding box의 12개 엣지 중 어느 것인지
/////////////////////////////////////////////////////////////////////////
float3 ComputeBoundingBoxPosition(uint bbInstanceID, uint edgeIndex, uint vertexID)
{
    FBoundingBoxData box = g_BoundingBoxes[bbInstanceID];
  
//    0: (bbMin.x, bbMin.y, bbMin.z)
//    1: (bbMax.x, bbMin.y, bbMin.z)
//    2: (bbMin.x, bbMax.y, bbMin.z)
//    3: (bbMax.x, bbMax.y, bbMin.z)
//    4: (bbMin.x, bbMin.y, bbMax.z)
//    5: (bbMax.x, bbMin.y, bbMax.z)
//    6: (bbMin.x, bbMax.y, bbMax.z)
//    7: (bbMax.x, bbMax.y, bbMax.z)
    int vertIndex = BB_EdgeIndices[edgeIndex][vertexID];
    float x = ((vertIndex & 1) == 0) ? box.bbMin.x : box.bbMax.x;
    float y = ((vertIndex & 2) == 0) ? box.bbMin.y : box.bbMax.y;
    float z = ((vertIndex & 4) == 0) ? box.bbMin.z : box.bbMax.z;
    return float3(x, y, z);
}

/////////////////////////////////////////////////////////////////////////
// Axis 위치 계산 함수 (총 3개: X, Y, Z)
/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////
// Cone 계산 함수
/////////////////////////////////////////////////
// Helper: 계산된 각도에 따른 밑면 꼭짓점 위치 계산

float3 ComputeConePosition(uint globalInstanceID, uint vertexID)
{
    // 모든 cone이 동일한 세그먼트 수를 가짐
    int N = g_ConeData[0].ConeSegmentCount;
    
    uint coneIndex = globalInstanceID / (2 * N);
    uint lineIndex = globalInstanceID % (2 * N);
    
    // cone 데이터 읽기
    FConeData cone = g_ConeData[coneIndex];
    
    // cone의 축 계산
    float3 axis = normalize(cone.ConeApex - cone.ConeBaseCenter);
    
    // axis에 수직인 두 벡터(u, v)를 생성
    float3 arbitrary = abs(dot(axis, float3(0, 0, 1))) < 0.99 ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 u = normalize(cross(axis, arbitrary));
    float3 v = cross(axis, u);
    
    if (lineIndex < (uint) N)
    {
        // 측면 선분: cone의 꼭짓점과 밑면의 한 점을 잇는다.
        float angle = lineIndex * 6.28318530718 / N;
        float3 baseVertex = cone.ConeBaseCenter + (cos(angle) * u + sin(angle) * v) * cone.ConeRadius;
        return (vertexID == 0) ? cone.ConeApex : baseVertex;
    }
    else
    {
        // 밑면 둘레 선분: 밑면상의 인접한 두 점을 잇는다.
        uint idx = lineIndex - N;
        float angle0 = idx * 6.28318530718 / N;
        float angle1 = ((idx + 1) % N) * 6.28318530718 / N;
        float3 v0 = cone.ConeBaseCenter + (cos(angle0) * u + sin(angle0) * v) * cone.ConeRadius;
        float3 v1 = cone.ConeBaseCenter + (cos(angle1) * u + sin(angle1) * v) * cone.ConeRadius;
        return (vertexID == 0) ? v0 : v1;
    }
}
/////////////////////////////////////////////////////////////////////////
// OBB
/////////////////////////////////////////////////////////////////////////
float3 ComputeOrientedBoxPosition(uint obIndex, uint edgeIndex, uint vertexID)
{
    FOrientedBoxCornerData ob = g_OrientedBoxes[obIndex];
    int cornerID = BB_EdgeIndices[edgeIndex][vertexID];
    return ob.corners[cornerID].xyz;
}

/////////////////////////////////////////////////////////////////////////
// Bone (Sphere + Square Pyramid)
/////////////////////////////////////////////////////////////////////////
float3 ComputeBonePosition(uint boneIndex, uint localInstanceID, uint vertexID)
{
    FBoneData bone = g_BoneData[boneIndex];
    
    static const float BONE_GIZMO_SPHERE_RADIUS = 5.0f; 
    
    uint circleId = localInstanceID / BONE_GIZMO_LINES_PER_CIRCLE; // 0 for XY, 1 for XZ, 2 for YZ
    uint segmentInCircle = localInstanceID % BONE_GIZMO_LINES_PER_CIRCLE; // 0 to (BONE_GIZMO_LINES_PER_CIRCLE - 1)
    
    float angle0 = (segmentInCircle / (float) BONE_GIZMO_LINES_PER_CIRCLE) * 2.0f * PI;
    float angle1 = ((segmentInCircle + 1.0f) / (float) BONE_GIZMO_LINES_PER_CIRCLE) * 2.0f * PI;
    
    float currentAngle = (vertexID == 0) ? angle0 : angle1;

    float3 pointOnCircle;
    
    if (circleId == 0) // XY plane circle
    {
        pointOnCircle.x = bone.Center.x + BONE_GIZMO_SPHERE_RADIUS * cos(currentAngle);
        pointOnCircle.y = bone.Center.y + BONE_GIZMO_SPHERE_RADIUS * sin(currentAngle);
        pointOnCircle.z = bone.Center.z;
    }
    else if (circleId == 1) // XZ plane circle
    {
        pointOnCircle.x = bone.Center.x + BONE_GIZMO_SPHERE_RADIUS * cos(currentAngle);
        pointOnCircle.y = bone.Center.y; // Y is constant for XZ plane
        pointOnCircle.z = bone.Center.z + BONE_GIZMO_SPHERE_RADIUS * sin(currentAngle);
    }
    else if (circleId == 2) // YZ plane circle
    {
        pointOnCircle.x = bone.Center.x; // X is constant for YZ plane
        pointOnCircle.y = bone.Center.y + BONE_GIZMO_SPHERE_RADIUS * cos(currentAngle);
        pointOnCircle.z = bone.Center.z + BONE_GIZMO_SPHERE_RADIUS * sin(currentAngle);
    }
    else
    {
        pointOnCircle = bone.Center;
    }
    return pointOnCircle;
}
/////////////////////////////////////////////////////////////////////////
// 메인 버텍스 셰이더
/////////////////////////////////////////////////////////////////////////
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float3 pos;
    float4 color;
    
    // Grid / Axis / AABB 인스턴스 개수 계산
    uint gridLineCount = GridCount; // 그리드 라인
    uint axisCount = 3; // X, Y, Z 축 (월드 좌표축)
    uint aabbInstanceCount = BoundingBoxCount * 12; // AABB 하나당 12개 엣지
    uint coneInstanceCount = ConeCount * 2 * g_ConeData[0].ConeSegmentCount;
    uint boneInstanceCount = BoneCount * BONE_GIZMO_TOTAL_LINES_PER_BONE; // Use the new global constant
    //uint obbInstanceCount = OBBCount * 12; // Using the count from CBuffer
    //uint instancesPerSphere = (NumSphereLatitudeLines * NumSphereSegmentsPerLine) + (NumSphereLongitudeLines * NumSphereSegmentsPerLine);
    
    // --- Calculate Start Indices ---
    uint gridEnd = gridLineCount;
    uint axisEnd = gridEnd + axisCount;
    uint aabbEnd = axisEnd + aabbInstanceCount;
    uint coneEnd = aabbEnd + coneInstanceCount;
    uint boneEnd = coneEnd + boneInstanceCount;
    //uint obbEnd = coneEnd + obbInstanceCount;
    //uint sphereEnd = obbEnd + sphereInstanceCount; // End of sphere instances
    //uint boxEnd = sphereEnd + boxInstanceCount;
    
    // --- Instance ID Branching ---
    if (input.instanceID < gridEnd)
    {
        // Grid
        pos = ComputeGridPosition(input.instanceID, input.vertexID);
        color = float4(0.1, 0.1, 0.1, 1.0);
    }
    else if (input.instanceID < axisEnd)
    {
        // Axis
        uint axisInstanceID = input.instanceID - gridEnd;
        pos = ComputeAxisPosition(axisInstanceID, input.vertexID);
        // Axis color assignment...
        if (axisInstanceID == 0)
            color = float4(1.0, 0.0, 0.0, 1.0); // X
        else if (axisInstanceID == 1)
            color = float4(0.0, 1.0, 0.0, 1.0); // Y
        else
            color = float4(0.0, 0.0, 1.0, 1.0); // Z
    }
    else if (input.instanceID < aabbEnd)
    {
        // AABB
        uint index = input.instanceID - axisEnd;
        uint bbInstanceID = index / 12;
        uint bbEdgeIndex = index % 12;
        pos = ComputeBoundingBoxPosition(bbInstanceID, bbEdgeIndex, input.vertexID);
        color = float4(1.0, 1.0, 0.0, 1.0); // Yellow
    }
    else if (input.instanceID < coneEnd)
    {
        // Cone
        uint coneInstanceID = input.instanceID - aabbEnd;
        pos = ComputeConePosition(coneInstanceID, input.vertexID);
        if (ConeCount > 0 && g_ConeData[0].ConeSegmentCount > 0)
        {
            int N = g_ConeData[0].ConeSegmentCount;
            uint coneIndex = coneInstanceID / (2 * N);
            if (coneIndex < ConeCount)
            {
                color = g_ConeData[coneIndex].Color;
            }
            else
            {
                color = float4(1, 0, 1, 1);
            }
        }
        else
        {
            color = float4(1, 0, 1, 1);
        }
    }
    else if (input.instanceID < boneEnd)
    {
        uint globalBoneLineID = input.instanceID - coneEnd;
        uint currentBoneDataIndex = globalBoneLineID / BONE_GIZMO_TOTAL_LINES_PER_BONE;
        uint localLineIDForBone = globalBoneLineID % BONE_GIZMO_TOTAL_LINES_PER_BONE;
        pos = ComputeBonePosition(currentBoneDataIndex, localLineIDForBone, input.vertexID);
        
        color = g_BoneData[currentBoneDataIndex].Color;
        //if (currentBoneDataIndex < BoneCount)
        //{
        //    color = g_BoneData[currentBoneDataIndex].Color;
        //}
        //else
        //{
        //    color = float4(1.0, 0.0, 1.0, 1.0);
        //}
    }
    //else if (input.instanceID < obbEnd)
    //{
    //    // Oriented Box (OBB)
    //    uint obbLocalID = input.instanceID - coneEnd;
    //    uint obbIndex = obbLocalID / 12;
    //    uint edgeIndex = obbLocalID % 12;
    //    // Ensure obbIndex is within bounds of g_OrientedBoxes if OrientedBoxCount > 0
    //    if (obbIndex < obbInstanceCount)
    //    {
    //        pos = ComputeOrientedBoxPosition(obbIndex, edgeIndex, input.vertexID);
    //    }
    //    else
    //    {
    //        pos = float3(0, 0, 0); // Error case
    //    }
    //    color = float4(0.4, 1.0, 0.4, 1.0); // Light Green
    //}
    else
    {
        // Fallback / Error case
        pos = float3(0.0, 0.0, 0.0);
        color = float4(1.0, 0.0, 1.0, 1.0); // Magenta for errors
    }

    // Output transformations
    output.Position = float4(pos, 1.f);
    output.Position = mul(output.Position, WorldMatrix); // Apply World matrix if needed (usually identity for world-space primitives)
    output.WorldPosition = output.Position;

    output.Position = mul(output.Position, ViewMatrix);
    output.Position = mul(output.Position, ProjectionMatrix);

    output.Color = color;
    output.instanceID = input.instanceID; // Pass instance ID
    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    if (input.instanceID < GridCount || input.instanceID < GridCount + 3)
    {
        float Dist = length(input.WorldPosition.xyz - ViewWorldLocation);

        float MaxDist = 400 * 1.2f;
        float MinDist = MaxDist * 0.3f;

         // Fade out grid
        float Fade = saturate(1.f - (Dist - MinDist) / (MaxDist - MinDist));
        input.Color.a *= Fade * Fade * Fade;
    }
    return input.Color;
}
