#include "ShadowManager.h"

#include "ViewportClient.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Math/JungleMath.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "Engine/Engine.h"
#include "World/World.h"

#include "GameFramework/PlayerController.h"

// --- 생성자 및 소멸자 ---

FShadowManager::FShadowManager()
{
    D3DDevice = nullptr;
    D3DContext = nullptr;
    ShadowSamplerCmp = nullptr;
    ShadowPointSampler = nullptr; // <<< 초기화 추가
    SpotShadowDepthRHI = nullptr;
    PointShadowCubeMapRHI = nullptr; // <<< 초기화 추가
    DirectionalShadowCascadeDepthRHI = nullptr;
}

FShadowManager::~FShadowManager()
{
    // 소멸 시 자동으로 리소스 해제 호출
    Release();
}



// --- Public 멤버 함수 구현 ---


bool FShadowManager::Initialize(FGraphicsDevice* InGraphics, FDXDBufferManager* InBufferManager,
    uint32_t InMaxSpotShadows, uint32_t InSpotResolution,
    uint32_t InMaxPointShadows, uint32_t InPointResolution, uint32_t InNumCascades, uint32_t InDirResolution)
{
    if (D3DDevice) // 이미 초기화된 경우 방지
    {
        Release();
    }

    if (!InGraphics || !InGraphics->Device || !InGraphics->DeviceContext)
    {
        // UE_LOG(LogTemp, Error, TEXT("FShadowManager::Initialize: Invalid GraphicsDevice provided."));
        return false;
    }

    D3DDevice = InGraphics->Device;
    D3DContext = InGraphics->DeviceContext;
    BufferManager = InBufferManager;

    // RHI 구조체 할당
    SpotShadowDepthRHI = new FShadowDepthRHI();
    PointShadowCubeMapRHI = new FShadowCubeMapArrayRHI(); // << 추가
    DirectionalShadowCascadeDepthRHI = new FShadowDepthRHI();

    // 설정 값 저장
    MaxSpotLightShadows = InMaxSpotShadows;
                           

    MaxPointLightShadows = InMaxPointShadows; // << 추가
    //NumCascades = InNumCascades; // 차후 명시적인 바인딩 위해 주석처리 

    SpotShadowDepthRHI->ShadowMapResolution = InSpotResolution;
    PointShadowCubeMapRHI->ShadowMapResolution = InPointResolution; // << 추가
    DirectionalShadowCascadeDepthRHI->ShadowMapResolution = InDirResolution;

    // 리소스 생성 시도
    if (!CreateSpotShadowResources())
    {
        // UE_LOG(LogTemp, Error, TEXT("Failed to create spot shadow resources!"));
        Release();
        return false;
    }
    if (!CreatePointShadowResources()) // << 추가된 호출
    {
        // UE_LOG(LogTemp, Error, TEXT("Failed to create point shadow resources!"));
        Release();
        return false;
    }
    if (!CreateDirectionalShadowResources())
    {
        // UE_LOG(LogTemp, Error, TEXT("Failed to create directional shadow resources!"));
        Release();
        return false;
    }
    if (!CreateSamplers())
    {
        // UE_LOG(LogTemp, Error, TEXT("Failed to create shadow samplers!"));
        Release();
        return false;
    }

    // 방향성 광원 ViewProj 행렬 배열 크기 설정
    CascadesViewProjMatrices.SetNum(NumCascades);

    // UE_LOG(LogTemp, Log, TEXT("FShadowManager Initialized Successfully."));
    return true;
}

void FShadowManager::Release()
{
    // 생성된 역순 또는 그룹별로 리소스 해제
    ReleaseSamplers();
    ReleaseDirectionalShadowResources();
    ReleasePointShadowResources(); // << 추가
    ReleaseSpotShadowResources();

    // 배열 클리어
    CascadesViewProjMatrices.Empty();

    // D3D 객체 포인터는 외부에서 관리하므로 여기서는 nullptr 처리만 함
    D3DDevice = nullptr;
    D3DContext = nullptr;
}

void FShadowManager::BeginSpotShadowPass(uint32_t sliceIndex)
{
    // 유효성 검사
    if (!D3DContext || sliceIndex >= (uint32_t)SpotShadowDepthRHI->ShadowDSVs.Num() || !SpotShadowDepthRHI->ShadowDSVs[sliceIndex])
    {
        // UE_LOG(LogTemp, Warning, TEXT("BeginSpotShadowPass: Invalid slice index or DSV."));
        return;
    }

    // 렌더 타겟 설정 (DSV만 설정)
    ID3D11RenderTargetView* nullRTV = nullptr;
    D3DContext->OMSetRenderTargets(1, &nullRTV, SpotShadowDepthRHI->ShadowDSVs[sliceIndex]);

    // 뷰포트 설정
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)SpotShadowDepthRHI->ShadowMapResolution;
    vp.Height = (float)SpotShadowDepthRHI->ShadowMapResolution;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    D3DContext->RSSetViewports(1, &vp);

    // DSV 클리어
    D3DContext->ClearDepthStencilView(SpotShadowDepthRHI->ShadowDSVs[sliceIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FShadowManager::BeginPointShadowPass(uint32_t sliceIndex)
{
    if (!D3DContext || !PointShadowCubeMapRHI || sliceIndex >= (uint32_t)PointShadowCubeMapRHI->ShadowDSVs.Num() || !PointShadowCubeMapRHI->ShadowDSVs[sliceIndex])
    {
        // UE_LOG(LogTemp, Warning, TEXT("BeginPointShadowPass: Invalid slice index (%u) or DSV."), sliceIndex);
        return; // 유효성 검사
    }

    // 포인트 라이트의 DSV 바인딩 (이 DSV는 TextureCubeArray의 특정 슬라이스(큐브맵)를 가리킴)
    ID3D11RenderTargetView* nullRTV = nullptr;
    D3DContext->OMSetRenderTargets(1, &nullRTV, PointShadowCubeMapRHI->ShadowDSVs[sliceIndex]);

    // 뷰포트 설정 (큐브맵 한 면의 해상도)
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)PointShadowCubeMapRHI->ShadowMapResolution;
    vp.Height = (float)PointShadowCubeMapRHI->ShadowMapResolution;
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    D3DContext->RSSetViewports(1, &vp);

    // DSV 클리어 (바인딩된 큐브맵 슬라이스의 모든 면을 클리어)
    D3DContext->ClearDepthStencilView(PointShadowCubeMapRHI->ShadowDSVs[sliceIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
}


void FShadowManager::BeginDirectionalShadowCascadePass(uint32_t cascadeIndex)
{
    // 유효성 검사
    if (!D3DContext || cascadeIndex >= (uint32_t)DirectionalShadowCascadeDepthRHI->ShadowDSVs.Num() || !DirectionalShadowCascadeDepthRHI->ShadowDSVs[cascadeIndex])
    {
         UE_LOG(LogLevel::Warning, TEXT("BeginDirectionalShadowCascadePass: Invalid cascade index or DSV."));
        return;
    }

    // 렌더 타겟 설정 (DSV만 설정)
    ID3D11RenderTargetView* nullRTV = nullptr;
    D3DContext->OMSetRenderTargets(1, &nullRTV, DirectionalShadowCascadeDepthRHI->ShadowDSVs[cascadeIndex]);

    // 뷰포트 설정
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)DirectionalShadowCascadeDepthRHI->ShadowMapResolution;
    vp.Height = (float)DirectionalShadowCascadeDepthRHI->ShadowMapResolution;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    D3DContext->RSSetViewports(1, &vp);

    // DSV 클리어
    D3DContext->ClearDepthStencilView(DirectionalShadowCascadeDepthRHI->ShadowDSVs[cascadeIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FShadowManager::BindResourcesForSampling(
    uint32_t spotShadowSlot, uint32_t pointShadowSlot, uint32_t directionalShadowSlot, // << pointShadowSlot 추가
    uint32_t samplerCmpSlot, uint32_t samplerPointSlot)
{
    if (!D3DContext) return;

    // SRV 바인딩
    if (SpotShadowDepthRHI && SpotShadowDepthRHI->ShadowSRV)
    {
        D3DContext->PSSetShaderResources(spotShadowSlot, 1, &SpotShadowDepthRHI->ShadowSRV);
    }
    if (PointShadowCubeMapRHI && PointShadowCubeMapRHI->ShadowSRV) // << 추가
    {
        D3DContext->PSSetShaderResources(pointShadowSlot, 1, &PointShadowCubeMapRHI->ShadowSRV);
    }
    if (DirectionalShadowCascadeDepthRHI && DirectionalShadowCascadeDepthRHI->ShadowSRV)
    {
        D3DContext->PSSetShaderResources(directionalShadowSlot, 1, &DirectionalShadowCascadeDepthRHI->ShadowSRV);

        FCascadeConstantBuffer CascadeData = {};
        CascadeData.World = FMatrix::Identity;
        for (uint32 i = 0; i < NumCascades; i++)
        {
            CascadeData.ViewProj[i] = CascadesViewProjMatrices[i];
            CascadeData.InvViewProj[i] = FMatrix::Inverse(CascadeData.ViewProj[i]);
            if (i >= CascadesInvProjMatrices.Num()) { continue; }
            CascadeData.InvProj[i] = CascadesInvProjMatrices[i];
        }

        if (CascadeSplits.Num() >= 4) {
            CascadeData.CascadeSplit = { CascadeSplits[0], CascadeSplits[1], CascadeSplits[2], CascadeSplits[3] };
        }
        /*CascadeData.CascadeSplits[i] = CascadeSplits[i];
        CascadeData.CascadeSplits[NumCascades] = CascadeSplits[NumCascades];*/

        /*BufferManager->UpdateConstantBuffer(TEXT("FCascadeConstantBuffer"), CascadeData);
        BufferManager->BindConstantBuffer(TEXT("FCascadeConstantBuffer"), 9, EShaderStage::Pixel);*/
        /*ID3D11Buffer* CascadeConstantBuffer = BufferManager->GetConstantBuffer(TEXT("FCascadeConstantBuffer"));
        D3DContext->PSSetConstantBuffers(9,1,&CascadeConstantBuffer);*/
    }

    // 샘플러 바인딩
    if (ShadowSamplerCmp)
    {
        D3DContext->PSSetSamplers(samplerCmpSlot, 1, &ShadowSamplerCmp);
    }
    if (ShadowPointSampler)
    {
        D3DContext->PSSetSamplers(samplerPointSlot, 1, &ShadowPointSampler);
    }
}

FMatrix FShadowManager::GetCascadeViewProjMatrix(int i) const
{
    if (i < 0 || i >= CascadesViewProjMatrices.Num())
    {
        UE_LOG(LogLevel::Warning, TEXT("GetCascadeViewProjMatrix: Invalid cascade index."));
        return FMatrix::Identity;
    }
    return CascadesViewProjMatrices[i];
}


// --- Private 멤버 함수 구현 (리소스 생성/해제 헬퍼) ---

bool FShadowManager::CreateSpotShadowResources()
{
    // 유효성 검사
    if (!D3DDevice || MaxSpotLightShadows == 0 || SpotShadowDepthRHI->ShadowMapResolution == 0) return false;

    // 1. Texture2DArray 생성
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = SpotShadowDepthRHI->ShadowMapResolution;
    texDesc.Height = SpotShadowDepthRHI->ShadowMapResolution;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = MaxSpotLightShadows;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS; // 깊이 포맷
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    HRESULT hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &SpotShadowDepthRHI->ShadowTexture);
    if (FAILED(hr))
    {
        // UE_LOG(LogTemp, Error, TEXT("CreateTexture2D failed for SpotShadowArrayTexture (HR=0x%X)"), hr);
        return false;
    }

    // 2. 전체 배열용 SRV 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 읽기용 포맷
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = MaxSpotLightShadows;

    hr = D3DDevice->CreateShaderResourceView(SpotShadowDepthRHI->ShadowTexture, &srvDesc, &SpotShadowDepthRHI->ShadowSRV);
    if (FAILED(hr))
    {
        // UE_LOG(LogTemp, Error, TEXT("CreateShaderResourceView failed for SpotShadowArraySRV (HR=0x%X)"), hr);
        ReleaseSpotShadowResources(); // 생성된 텍스처 정리
        return false;
    }

    
    SpotShadowDepthRHI->ShadowDSVs.SetNum(MaxSpotLightShadows);
    for (uint32_t i = 0; i < MaxSpotLightShadows; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 깊이 포맷
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Flags = 0;

        hr = D3DDevice->CreateDepthStencilView(SpotShadowDepthRHI->ShadowTexture, &dsvDesc, &SpotShadowDepthRHI->ShadowDSVs[i]);
        if (FAILED(hr))
        {
            // UE_LOG(LogTemp, Error, TEXT("CreateDepthStencilView failed for SpotShadowSliceDSV[%u] (HR=0x%X)"), i, hr);
            ReleaseSpotShadowResources(); // 생성된 것들 정리
            return false;
        }
    }

    SpotShadowDepthRHI->ShadowSRVs.SetNum(MaxSpotLightShadows);
    for (uint32_t i = 0; i < MaxSpotLightShadows; ++i)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = i;
        srvDesc.Texture2DArray.ArraySize = 1;

        hr = D3DDevice->CreateShaderResourceView(SpotShadowDepthRHI->ShadowTexture, &srvDesc, & SpotShadowDepthRHI->ShadowSRVs[i]);
        if (FAILED(hr)) { ReleaseDirectionalShadowResources(); return false; }
    }

    return true;
}

void FShadowManager::ReleaseSpotShadowResources()
{
    if (SpotShadowDepthRHI) { SpotShadowDepthRHI->Release();
        delete SpotShadowDepthRHI;
        SpotShadowDepthRHI = nullptr;
    }
}

bool FShadowManager::CreatePointShadowResources() // << 추가된 함수 구현
{
    if (!D3DDevice || !PointShadowCubeMapRHI || MaxPointLightShadows == 0 || PointShadowCubeMapRHI->ShadowMapResolution == 0) return false;

    // 1. TextureCubeArray 생성 (Texture2D로 생성 후 MiscFlags 사용)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = PointShadowCubeMapRHI->ShadowMapResolution;
    texDesc.Height = PointShadowCubeMapRHI->ShadowMapResolution;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = MaxPointLightShadows * 6; // <<< 총 면의 개수 (큐브맵 개수 * 6)
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;     // TYPELESS 사용
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // <<< 큐브맵임을 명시

    HRESULT hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &PointShadowCubeMapRHI->ShadowTexture);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Shadow Cube Map texture!"));
        return hr;
    }

    // 2. 전체 배열용 SRV 생성 (TEXTURECUBEARRAY 사용)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 읽기 형식
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY; // <<< 큐브맵 배열 뷰
    srvDesc.TextureCubeArray.MostDetailedMip = 0;
    srvDesc.TextureCubeArray.MipLevels = 1;
    srvDesc.TextureCubeArray.First2DArrayFace = 0;      // 첫 번째 큐브맵의 인덱스
    srvDesc.TextureCubeArray.NumCubes = MaxPointLightShadows; // <<< 총 큐브맵 개수

    hr = D3DDevice->CreateShaderResourceView(PointShadowCubeMapRHI->ShadowTexture, &srvDesc, &PointShadowCubeMapRHI->ShadowSRV);
    if (FAILED(hr))
    {
        // UE_LOG(LogTemp, Error, TEXT("CreateShaderResourceView failed for PointShadowCubeMap SRV (HR=0x%X)"), hr);
        ReleasePointShadowResources();
        return false;
    }

    // 3. 각 큐브맵 슬라이스용 DSV 생성 (렌더링 시 GS의 SV_RenderTargetArrayIndex 사용)
    PointShadowCubeMapRHI->ShadowDSVs.SetNum(MaxPointLightShadows);
    for (uint32_t i = 0; i < MaxPointLightShadows; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 쓰기 형식
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY; // DSV는 2D Array View 사용
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i * 6; // <<< 각 큐브맵의 시작 인덱스 (i번째 큐브맵의 +X 면)
        dsvDesc.Texture2DArray.ArraySize = 6;       // <<< 각 DSV는 큐브맵 하나(6면)를 가리킴
        dsvDesc.Flags = 0;

        hr = D3DDevice->CreateDepthStencilView(PointShadowCubeMapRHI->ShadowTexture, &dsvDesc, &PointShadowCubeMapRHI->ShadowDSVs[i]);
        if (FAILED(hr))
        {
            // UE_LOG(LogTemp, Error, TEXT("CreateDepthStencilView failed for PointShadowCubeMap DSV[%u] (HR=0x%X)"), i, hr);
            ReleasePointShadowResources();
            return false;
        }
    }

    // --- 4. ImGui 디버그용: 각 면에 대한 개별 SRV 생성 ---
    PointShadowCubeMapRHI->ShadowFaceSRVs.SetNum(MaxPointLightShadows); // 외부 배열 크기 설정
    for (uint32_t i = 0; i < MaxPointLightShadows; ++i) // 각 포인트 라이트 루프
    {
        PointShadowCubeMapRHI->ShadowFaceSRVs[i].SetNum(6); // 내부 배열 크기 설정 (6개 면)
        for (uint32_t j = 0; j < 6; ++j) // 각 면 루프 (+X, -X, +Y, -Y, +Z, -Z 순서 가정)
        {
            // 이 면의 플랫 배열 인덱스 계산
            uint32_t flatArrayIndex = i * 6 + j;

            D3D11_SHADER_RESOURCE_VIEW_DESC faceSrvDesc = {};
            faceSrvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 읽기 형식
            faceSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; // 소스는 배열이지만, 뷰는 단일 슬라이스
            faceSrvDesc.Texture2DArray.MostDetailedMip = 0;
            faceSrvDesc.Texture2DArray.MipLevels = 1;
            faceSrvDesc.Texture2DArray.FirstArraySlice = flatArrayIndex; // <<< 이 면의 인덱스 지정
            faceSrvDesc.Texture2DArray.ArraySize = 1; // <<< SRV는 단 1개의 슬라이스만 가리킴

            hr = D3DDevice->CreateShaderResourceView(PointShadowCubeMapRHI->ShadowTexture, &faceSrvDesc, &PointShadowCubeMapRHI->ShadowFaceSRVs[i][j]);
            if (FAILED(hr))
            {
                // UE_LOG(LogTemp, Error, TEXT("CreateShaderResourceView failed for PointShadow Face SRV [%u][%u] (HR=0x%X)"), i, j, hr);
                // 실패 시, 지금까지 생성된 모든 리소스 정리 필요 (더 복잡한 롤백 로직 또는 단순하게 전체 해제)
                ReleasePointShadowResources();
                return false;
            }
        }
    }

    return true;
}

void FShadowManager::ReleasePointShadowResources() // << 추가된 함수 구현
{
    if (PointShadowCubeMapRHI)
    {
        PointShadowCubeMapRHI->Release();
        delete PointShadowCubeMapRHI; // Initialize에서 new 했으므로 delete 필요
        PointShadowCubeMapRHI = nullptr;
    }
}

bool FShadowManager::CreateDirectionalShadowResources()
{
    // 유효성 검사
    if (!D3DDevice || NumCascades == 0 || DirectionalShadowCascadeDepthRHI->ShadowMapResolution == 0) return false;

    // 1. Texture2DArray 생성 (CSM 용)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = DirectionalShadowCascadeDepthRHI->ShadowMapResolution;
    texDesc.Height = DirectionalShadowCascadeDepthRHI->ShadowMapResolution;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = NumCascades; // 캐스케이드 개수만큼
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &DirectionalShadowCascadeDepthRHI->ShadowTexture);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Shadow Cube Map texture!"));
        return hr;
    }

    // 2. 전체 배열용 SRV 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = NumCascades;

    hr = D3DDevice->CreateShaderResourceView(DirectionalShadowCascadeDepthRHI->ShadowTexture, &srvDesc, &DirectionalShadowCascadeDepthRHI->ShadowSRV);
    if (FAILED(hr)) { ReleaseDirectionalShadowResources(); return false; }

    // 3. 각 캐스케이드용 DSV 생성
    DirectionalShadowCascadeDepthRHI->ShadowDSVs.SetNum(1);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.MipSlice = 0;
    dsvDesc.Texture2DArray.FirstArraySlice = 0;
    dsvDesc.Texture2DArray.ArraySize = NumCascades;

    hr = D3DDevice->CreateDepthStencilView(DirectionalShadowCascadeDepthRHI->ShadowTexture, &dsvDesc, &DirectionalShadowCascadeDepthRHI->ShadowDSVs[0]);
    if (FAILED(hr)) { ReleaseDirectionalShadowResources(); return false; }
    /*DirectionalShadowCascadeDepthRHI->ShadowDSVs.SetNum(NumCascades);
    for (uint32_t i = 0; i < NumCascades; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;

        hr = D3DDevice->CreateDepthStencilView(DirectionalShadowCascadeDepthRHI->ShadowTexture, &dsvDesc, &DirectionalShadowCascadeDepthRHI->ShadowDSVs[i]);
        if (FAILED(hr)) { ReleaseDirectionalShadowResources(); return false; }
    }*/

    // Directional Light의 Shadow Map 개수 = Cascade 개수 (분할 개수)
    DirectionalShadowCascadeDepthRHI->ShadowSRVs.SetNum(NumCascades); 
    for (uint32_t i = 0; i < NumCascades; ++i)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = i;
        srvDesc.Texture2DArray.ArraySize = 1;

        hr = D3DDevice->CreateShaderResourceView(DirectionalShadowCascadeDepthRHI->ShadowTexture, &srvDesc, & DirectionalShadowCascadeDepthRHI->ShadowSRVs[i]);
        if (FAILED(hr)) { ReleaseDirectionalShadowResources(); return false; }
    }
    
    
    return true;
}

void FShadowManager::ReleaseDirectionalShadowResources()
{
    if (DirectionalShadowCascadeDepthRHI)
    {
        DirectionalShadowCascadeDepthRHI->Release();
        delete DirectionalShadowCascadeDepthRHI; // Initialize에서 new 했으므로 delete 필요
        DirectionalShadowCascadeDepthRHI = nullptr;
    }
}

void FShadowManager::UpdateCascadeMatrices(const std::shared_ptr<FViewportClient>& Viewport, UDirectionalLightComponent* DirectionalLight)
{
    CascadesViewProjMatrices.Empty();
    CascadesInvProjMatrices.Empty();

    FMatrix CamView;
    float NearClip;
    float FarClip;
    float FOV;
    float AspectRatio;
    
    if (GEngine->ActiveWorld->WorldType == EWorldType::Editor)
    {
        CamView = Viewport->GetViewMatrix();
        NearClip = Viewport->GetNearClip();
        FarClip = Viewport->GetFarClip();
        FOV = Viewport->GetFieldOfView();          // Degrees
        AspectRatio = Viewport->GetAspectRatio();
    }
    else if (GEngine->ActiveWorld->WorldType == EWorldType::PIE)
    {
        auto CameraPOV = GEngine->ActiveWorld->GetFirstPlayerController()->PlayerCameraManager->ViewTarget.POV;

        CamView = JungleMath::CreateViewMatrix(
            CameraPOV.Location,
            CameraPOV.Location + CameraPOV.Rotation.GetForwardVector(),
            CameraPOV.Rotation.GetUpVector()
        );
        
        if (CameraPOV.ProjectionMode == ECameraProjectionMode::Perspective)
        {
            NearClip = CameraPOV.PerspectiveNearClipPlane;
            FarClip = CameraPOV.PerspectiveFarClipPlane;
        }
        else if (CameraPOV.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            NearClip = CameraPOV.OrthoNearClipPlane;
            FarClip = CameraPOV.OrthoFarClipPlane;
        }
    
        FOV = CameraPOV.FOV;          // Degrees
        AspectRatio = CameraPOV.AspectRatio;
    }

	float halfHFOV = FMath::DegreesToRadians(FOV) * 0.5f;
	float tanHFOV = FMath::Tan(halfHFOV);
	float tanVFOV = tanHFOV / AspectRatio;
	FMatrix InvView = FMatrix::Inverse(CamView);

    //CascadeSplits.Empty();
    CascadeSplits.SetNum(NumCascades + 1);
    CascadeSplits[0] = NearClip;
    CascadeSplits[NumCascades] = FarClip;
    for (uint32 i = 1; i < NumCascades; ++i)
    {
        float p = float(i) / float(NumCascades);
        float logSplit = NearClip * powf(FarClip / NearClip, p);      // 로그 분포
        float uniSplit = NearClip + (FarClip - NearClip) * p;         // 균등 분포
        CascadeSplits[i] = 0.7f * logSplit + 0.3f * uniSplit;         // 혼합 (0.5:0.5)
    }

	// 4) LightDir, Up
	const FVector LightDir = DirectionalLight->GetDirection().GetSafeNormal();
	FVector Up = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(LightDir, FVector::UpVector)) > 0.99f)
		Up = FVector::ForwardVector;

	CascadesViewProjMatrices.Empty();

    for (uint32 c = 0; c< NumCascades; ++c)
    {
		// i 단계의 Near / Far (월드 단위) 계산
		float splitN = CascadeSplits[c];
		float splitF = CascadeSplits[c + 1];
		float zn = (splitN - NearClip) / (FarClip - NearClip);
        float zf = (splitF - NearClip) / (FarClip - NearClip);

		// 뷰 공간 평면상의 X,Y 절댓값
		float nx = tanHFOV * splitN;
		float ny = tanVFOV * splitN;
		float fx = tanHFOV * splitF;
		float fy = tanVFOV * splitF;

		// 뷰 공간 8개 코너
		FVector ViewCorners[8] = {
			{ -nx,  ny, splitN },
			{  nx,  ny, splitN },
			{  nx, -ny, splitN },
			{ -nx, -ny, splitN },
			{ -fx,  fy, splitF },
			{  fx,  fy, splitF },
			{  fx, -fy, splitF },
			{ -fx, -fy, splitF }
		};

		// 월드 공간으로 변환
		FVector WorldCorners[8];
		for (int i = 0; i < 8; ++i)
		{
			// TransformPosition 은 w=1 가정 + divide 처리
			WorldCorners[i] = InvView.TransformPosition(ViewCorners[i]);
		}

        // Light Space AABB
        FMatrix LightView = DirectionalLight->GetViewMatrix();
		LightView.M[3][0] = LightView.M[3][1] = LightView.M[3][2] = 0; // translation 제거 - Rot만 필요

        FVector Min(FLT_MAX), Max(-FLT_MAX);
		for (auto& World : WorldCorners) {
			Min.X = FMath::Min(Min.X, World.X);  Max.X = FMath::Max(Max.X, World.X);
			Min.Y = FMath::Min(Min.Y, World.Y);  Max.Y = FMath::Max(Max.Y, World.Y);
			Min.Z = FMath::Min(Min.Z, World.Z);  Max.Z = FMath::Max(Max.Z, World.Z);
		}
		FVector CenterWS = (Min + Max) * 0.5f;
		float Radius = FMath::Max3(Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z) * 0.5f;

		// 3. Light View 생성
		FVector Eye = CenterWS - LightDir * Radius;
		LightView = JungleMath::CreateViewMatrix(Eye, CenterWS, Up);

		// 4. 모든 WorldCorners를 LightView로 변환, LightSpace에서의 Min/Max 구함
		FVector MinLS(FLT_MAX), MaxLS(-FLT_MAX);
		for (auto& World : WorldCorners) {
			FVector LS = LightView.TransformPosition(World);
			MinLS.X = FMath::Min(MinLS.X, LS.X);  MaxLS.X = FMath::Max(MaxLS.X, LS.X);
			MinLS.Y = FMath::Min(MinLS.Y, LS.Y);  MaxLS.Y = FMath::Max(MaxLS.Y, LS.Y);
			MinLS.Z = FMath::Min(MinLS.Z, LS.Z);  MaxLS.Z = FMath::Max(MaxLS.Z, LS.Z);
		}
		float Zm = (MaxLS.Z - MinLS.Z) * 0.1f;
        MinLS.X -= Zm; MinLS.Y -= Zm; MinLS.Z -= Zm;
        MaxLS.X -= Zm; MaxLS.Y -= Zm; MaxLS.Z -= Zm;

		// 5. LightSpace에서 Ortho 행렬 생성
		FMatrix LightProj = JungleMath::CreateOrthoProjectionMatrix(
            MaxLS.X- MinLS.X,
            MaxLS.Y - MinLS.Y,
			MinLS.Z, MaxLS.Z
		);

		// 6. 최종 ViewProj 행렬
		CascadesViewProjMatrices.Add(LightView * LightProj);
        CascadesInvProjMatrices.Add(FMatrix::Inverse(LightProj));
    }

}

bool FShadowManager::CreateSamplers()
{
    if (!D3DDevice) return false;

    // 1. Comparison Sampler (PCF 용)
    D3D11_SAMPLER_DESC sampDescCmp = {};
    sampDescCmp.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 선형 필터링 비교
    sampDescCmp.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDescCmp.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDescCmp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDescCmp.MipLODBias = 0.0f;
    sampDescCmp.MaxAnisotropy = 1;
    sampDescCmp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 깊이 비교 함수
    sampDescCmp.BorderColor[0] = 1.0f; // 경계 밖 = 깊이 최대 (그림자 없음)
    sampDescCmp.BorderColor[1] = 1.0f;
    sampDescCmp.BorderColor[2] = 1.0f;
    sampDescCmp.BorderColor[3] = 1.0f;
    sampDescCmp.MinLOD = 0;
    sampDescCmp.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = D3DDevice->CreateSamplerState(&sampDescCmp, &ShadowSamplerCmp);
    if (FAILED(hr)) return false;

    // 2. Point Sampler (하드 섀도우 또는 VSM/ESM 등)
    D3D11_SAMPLER_DESC PointSamplerDesc = {};
    PointSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    PointSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    PointSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    PointSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    PointSamplerDesc.BorderColor[0] = 1.0f; PointSamplerDesc.BorderColor[1] = 1.0f; PointSamplerDesc.BorderColor[2] = 1.0f; PointSamplerDesc.BorderColor[3] = 1.0f; // 높은 깊이 값
    PointSamplerDesc.MinLOD = 0;
    PointSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    PointSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER; // 비교 안 함

    hr = D3DDevice->CreateSamplerState(&PointSamplerDesc, &ShadowPointSampler);
    if (FAILED(hr))
    {
        // UE_LOG(LogLevel::Error, TEXT("Failed to create Shadow Point Sampler!"));
        ReleaseSamplers(); // 생성된 Comparison 샘플러 해제
        return false;
    }

    return true;
}

void FShadowManager::ReleaseSamplers()
{
    if (ShadowSamplerCmp) { ShadowSamplerCmp->Release(); ShadowSamplerCmp = nullptr; }
    if (ShadowPointSampler) { ShadowPointSampler->Release(); ShadowPointSampler = nullptr; }
    //if (ShadowSampler) { ShadowSampler->Release(); ShadowSampler = nullptr; }
}
