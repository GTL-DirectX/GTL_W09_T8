#pragma once

// 이거는 Slate를 쓰기는 좀 그렇고, 그런데 ImGui를 쓸건데 굳이 이걸 따로 만드는게 좋을까 고민중
// 안만들면 class explosion

class ViewerEditor
{
public:
    static void CreateViewerWindow();

    void RenderSkeletalMesh(USkeletalMesh* SkeletalMeshComp) const;
};

