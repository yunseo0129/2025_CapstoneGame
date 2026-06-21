#include "Bullets.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"
#include "Map.h"
#include "Collider.h"

CBullets::CBullets(EngineContext* _pcontext)
    : CGameObject{ _pcontext }
{

}

CBullets::CBullets(const CBullets& Prototype)
    : CGameObject(Prototype.m_pContext)
{

}

HRESULT CBullets::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CBullets::Initialize(void* pArg)
{
    m_vBullets.resize(100);

    if (nullptr == pArg)
        return E_FAIL;

    BULLET_DESC* pDesc = static_cast<BULLET_DESC*>(pArg);
   
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void CBullets::Priority_Update(_float fTimeDelta)
{
    __super::Priority_Update(fTimeDelta);
    for (BULLET& b : m_vBullets)
    {
        if (b.isOn)
        {
            b.fLifeTime -= fTimeDelta;
            if (b.fLifeTime <= 0.f)
                b.isOn = false;
        }
    }
}

void CBullets::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);
}

void CBullets::Late_Update(_float fTimeDelta)
{
    Cull_And_Submit(CRenderer::RG_NONBLEND);
    __super::Late_Update(fTimeDelta);
}

void CBullets::Render(ID3D12GraphicsCommandList* _commandList)
{
    for (BULLET& b : m_vBullets)
    {
        if (b.isOn)
        {
            // 여기서 렌더
            b.vTarget; // 렌더 되어야할 위치
        }
    }

    //_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_matFPSModel, 0);

    //// 2. PSO 설정
    //m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::ANIM);

    //// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
    //_uint iNumMeshes = m_pFPSModelCom->Get_NumMeshes();
    //for (_uint i = 0; i < iNumMeshes; ++i)
    //{
    //    m_pFPSModelCom->Bind_BoneMatrices(_commandList, i);
    //    m_pFPSModelCom->Render(_commandList, i);
    //}
}

void CBullets::NewBullet(BULLET _bullet)
{
    for (BULLET& b : m_vBullets)
    {
        if (!b.isOn)
        {
            b = _bullet;
            return;
        }
    }

    // 한도 초과시
    BULLET b = _bullet;
    m_vBullets.push_back(b);
}

HRESULT CBullets::Ready_Components()
{
    
    return S_OK;
}

CBullets* CBullets::Create(EngineContext* _pcontext)
{
    CBullets* pInstance = new CBullets(_pcontext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CBullets");
    }
    return pInstance;
}

CGameObject* CBullets::Clone(void* pArg)
{
    CBullets* pInstance = new CBullets(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CBullets");
    }
    return pInstance;
}

void CBullets::Free()
{
    __super::Free();
}