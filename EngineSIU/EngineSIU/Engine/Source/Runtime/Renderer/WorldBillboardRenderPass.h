#pragma once

#include "BillboardRenderPass.h"

class FWorldBillboardRenderPass : public FBillboardRenderPass
{
public:
    FWorldBillboardRenderPass();
    virtual ~FWorldBillboardRenderPass() = default;

    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;
};
