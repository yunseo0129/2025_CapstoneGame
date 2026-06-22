#include "CharSelect_Fish.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Model.h"

CCharSelect_Fish::CCharSelect_Fish(EngineContext* pContext)
    : CGameObject(pContext)
{
}

CCharSelect_Fish::CCharSelect_Fish(const CCharSelect_Fish& Prototype)
    : CGameObject(Prototype)
{
}

HRESULT CCharSelect_Fish::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CCharSelect_Fish::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    CHARSELECT_FISH_DESC* pDesc = static_cast<CHARSELECT_FISH_DESC*>(pArg);

    m_strModelTag = pDesc->strModelTag;
    m_iModelLevelIndex = pDesc->iModelLevelIndex;
    m_iAnimIndex = pDesc->iAnimIndex;

    // Transform 컴포넌트가 쓰는 속도값(없어도 무방하지만 안전하게 세팅)
    pDesc->fSpeedPerSec = 1.f;
    pDesc->fRotationPerSec = 1.f;

    // 베이스에서 Transform 생성
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    // 받침대 위 배치 (크기 → 회전 → 위치)
    m_pTransformCom->Scaling(pDesc->vScale.x, pDesc->vScale.y, pDesc->vScale.z);
    m_pTransformCom->RotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);
    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(pDesc->vPos.x, pDesc->vPos.y, pDesc->vPos.z, 1.f));

    if (FAILED(Ready_Components()))
        return E_FAIL;

    // 상/하체 분리 애니메이션 셋업 (Fish 모델도 상하체 분리 구조)
    m_pModelCom->Set_SpineBoneIndex(m_pModelCom->Get_BoneIndex("spine"));
    m_pModelCom->SetUp_Animation(m_iAnimIndex, true, true);   // 상체, 루프
    m_pModelCom->SetUp_Animation(m_iAnimIndex, true, false);  // 하체, 루프

    return S_OK;
}

void CCharSelect_Fish::Priority_Update(_float fTimeDelta)
{
}

void CCharSelect_Fish::Update(_float fTimeDelta)
{
    // 제자리 idle 재생 (입력/블렌딩 로직 없음)
    m_pModelCom->Play_Animation(fTimeDelta, true);
    m_pModelCom->Play_Animation(fTimeDelta, false);
    m_pModelCom->Merge_UpperLower();
}

void CCharSelect_Fish::Late_Update(_float fTimeDelta)
{
    // Get_WorldBoundingSphere 를 오버라이드하지 않으므로(=false 반환)
    // 컬링 없이 항상 메인/그림자 큐에 제출된다.
    Cull_And_Submit(CRenderer::RG_NONBLEND);
}

void CCharSelect_Fish::Render(ID3D12GraphicsCommandList* _commandList)
{
    XMFLOAT4X4 WorldMatrix;
    XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::ANIM);

    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        m_pModelCom->Bind_BoneMatrices(_commandList, i);
        m_pModelCom->Render(_commandList, i);
    }
}

void CCharSelect_Fish::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
    XMFLOAT4X4 WorldMatrix;
    XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SHADOW_ANIM);

    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        m_pModelCom->Bind_BoneMatrices(_commandList, i);
        m_pModelCom->Render(_commandList, i, true);
    }
}

HRESULT CCharSelect_Fish::Ready_Components()
{
    // 애니메이션 모델 → Clone(깊은 복사)로 인스턴스 전용 뼈 상태 확보
    if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
        TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
    {
        MSG_BOX("Failed to Add Component : Model in CCharSelect_Fish");
        return E_FAIL;
    }
    return S_OK;
}

CCharSelect_Fish* CCharSelect_Fish::Create(EngineContext* pContext)
{
    CCharSelect_Fish* pInstance = new CCharSelect_Fish(pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : CCharSelect_Fish");
        Safe_Release(pInstance);
    }
    return pInstance;
}

CGameObject* CCharSelect_Fish::Clone(void* pArg)
{
    CCharSelect_Fish* pInstance = new CCharSelect_Fish(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Clone : CCharSelect_Fish");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CCharSelect_Fish::Free()
{
    // m_pModelCom 은 Add_Component 로 m_Components 에 등록되어 있어
    // CGameObject::Free 가 알아서 Release 한다. (별도 해제 불필요)
    __super::Free();
}