#include "CubeComp.h"
#include "Engine/ObjLoader.h"
#include "UObject/ObjectFactory.h"


UCubeComp::UCubeComp()
{
    SetType(StaticClass()->GetName());
    AABB.max = { 1,1,1 };
    AABB.min = { -1,-1,-1 };

}

void UCubeComp::InitializeComponent()
{
    Super::InitializeComponent();

    //FObjManager::CreateStaticMesh("Assets/helloBlender.obj");
    //SetStaticMesh(FObjManager::GetStaticMesh(L"helloBlender.obj"));
    // 
    // Begin Test
    FObjManager::CreateStaticMesh("Contents/Reference/Reference.obj");
    SetStaticMesh(FObjManager::GetStaticMesh(L"Reference.obj"));
    // End Test
}

void UCubeComp::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

}
