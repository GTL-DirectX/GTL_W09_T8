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

    FObjManager::CreateStaticMesh("Contents/Reference/Reference.obj");
    SetStaticMesh(FObjManager::GetStaticMesh(L"Reference.obj"));
}

void UCubeComp::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

}
