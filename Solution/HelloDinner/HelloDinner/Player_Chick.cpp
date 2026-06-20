#include "Player_Chick.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Ketchup_Gun.h"

CPlayer_Chick::CPlayer_Chick(EngineContext* _pcontext)
    : CContainerObj{ _pcontext }
{

}

CPlayer_Chick::CPlayer_Chick(const CPlayer_Chick& Prototype)
    : CContainerObj(Prototype.m_pContext)
{

}

HRESULT CPlayer_Chick::Initialize_Prototype()
{
    return S_OK;
}
static int a = 0;
HRESULT CPlayer_Chick::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    PLAYER_CHICK_DESC* pDesc = static_cast<PLAYER_CHICK_DESC*>(pArg);
    m_strModelTag = pDesc->strModelTag;
    m_iModelLevelIndex = pDesc->iModelLevelIndex;
    pDesc->iNumPartObj = 1;
    pDesc->fSpeedPerSec = 1.f;
    pDesc->fRotationPerSec = 1.f;
    pDesc->fSpeedPerSec = 1.f;
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    m_pTransformCom->Scaling(1.f, 1.f, 1.f);
    m_pTransformCom->RotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);
    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(pDesc->vPos.x, pDesc->vPos.y, pDesc->vPos.z, 1.f));

    if (FAILED(Ready_Components()))
        return E_FAIL;

    m_iState = 0;
    m_pModelCom->SetUp_Animation(8, true, true);
    m_pModelCom->SetUp_Animation(8, true, false);

    if (FAILED(Ready_PartObjects()))
        return E_FAIL;

    return S_OK;
}

void CPlayer_Chick::Priority_Update(_float fTimeDelta)
{
    __super::Priority_Update(fTimeDelta);
}

void CPlayer_Chick::Update(_float fTimeDelta)
{
    if (!m_pModelCom->Get_UpperBlend())
        m_pModelCom->Play_Animation(fTimeDelta, true);
    else
        m_pModelCom->Blend_Animation(fTimeDelta, true);

    if (!m_pModelCom->Get_LowerBlend())
        m_pModelCom->Play_Animation(fTimeDelta, false);
    else
        m_pModelCom->Blend_Animation(fTimeDelta, false);

    Anim_Test();

    m_pModelCom->Merge_UpperLower();

    __super::Update(fTimeDelta);
}

void CPlayer_Chick::Late_Update(_float fTimeDelta)
{
    Cull_And_Submit(CRenderer::RG_NONBLEND);
    __super::Late_Update(fTimeDelta);
}

void CPlayer_Chick::Render(ID3D12GraphicsCommandList* _commandList)
{
    XMFLOAT4X4 WorldMatrix;
    XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

    // 2. PSO 설정
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::ANIM);

    // 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        m_pModelCom->Bind_BoneMatrices(_commandList, i);
        m_pModelCom->Render(_commandList, i);
    }
}

void CPlayer_Chick::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
    // Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
    XMFLOAT4X4 WorldMatrix;
    XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

    // 2. PSO 설정
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SHADOW_ANIM);

    // 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        m_pModelCom->Bind_BoneMatrices(_commandList, i);
        m_pModelCom->Render(_commandList, i, true);
    }

}

HRESULT CPlayer_Chick::Ready_PartObjects()
{
    // 케첩건
    {
        CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
        cdesc.strModelTag = L"Prototype_Component_ketchupGun";
        cdesc.iModelLevelIndex = m_iModelLevelIndex;
        cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
        cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("weapon.R");
        cdesc.vScale = _float3(1.f, 1.f, 1.f);
        m_PartObjects[0] = static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, m_iModelLevelIndex, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc));
        if (nullptr == m_PartObjects[0])
            return E_FAIL;
    }

    // 마요네즈건
    {
        CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
        cdesc.strModelTag = L"Prototype_Component_MayonaiseGun";
        cdesc.iModelLevelIndex = m_iModelLevelIndex;
        cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();;
        cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("weapon.R");
        cdesc.vScale = _float3(1.f, 1.f, 1.f);
        m_PartObjects.push_back(static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, m_iModelLevelIndex, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc)));
        if (nullptr == m_PartObjects[1])
            return E_FAIL;

        m_PartObjects[1]->SetOnOff(false);
    }
    return S_OK;
}

HRESULT CPlayer_Chick::Ready_Components()
{
    // Model 컴포넌트 생성
    if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
        TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
    {
        MSG_BOX("Failed to Add Component : Model in CPlayer_1rd");
        return E_FAIL;
    }

    m_pModelCom->Set_SpineBoneIndex(m_pModelCom->Get_BoneIndex("spine"));

    return S_OK;
}

void CPlayer_Chick::Anim_Test()
{
    if (m_pGameInstance->Key_Pressing(DIK_0))
    {
        // 정지 
        // 풀
        m_pModelCom->Change_Animation(0, 5.f, true, true);
        m_pModelCom->Change_Animation(0, 5.f, true, false);
    }
    if (m_pGameInstance->Key_Pressing(DIK_1))
    {
        // 대기
        // 풀
        m_pModelCom->Change_Animation(1, 5.f, true, true);
        m_pModelCom->Change_Animation(2, 5.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_2))
    {
        // 사격
        // 상체
        m_pModelCom->Change_Animation(3, 0.f, false, true);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_3))
    {
        // 앉기 정지
        // 하체
        m_pModelCom->Change_Animation(4, 2.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_4))
    {
        // 앉기 걷기
        // 하체
        m_pModelCom->Change_Animation(5, 1.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_5))
    {
        // 걷기
        // 하체
        m_pModelCom->Change_Animation(8, 1.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_6))
    {
        // 달리기
        // 하체
        m_pModelCom->Change_Animation(10, 0.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_7))
    {
        // 죽기
        // 풀 
        m_pModelCom->Change_Animation(9, 5.f, false, true);
        m_pModelCom->Change_Animation(9, 5.f, false, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_8))
    {
        // 장전
        // 상체
        m_pModelCom->Change_Animation(12, 3.f, false, true);
        static_cast<CKetchup_Gun*>(m_PartObjects[0])->Get_Model()->Change_Animation(2, 3.f, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_9))
    {
        // 점프
        // 풀
        m_pModelCom->Change_Animation(11, 5.f, false, true);
        m_pModelCom->Change_Animation(11, 5.f, false, false);
    }
}

CPlayer_Chick* CPlayer_Chick::Create(EngineContext* _pcontext)
{
    CPlayer_Chick* pInstance = new CPlayer_Chick(_pcontext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CPlayer_Chick");
    }
    return pInstance;
}

CGameObject* CPlayer_Chick::Clone(void* pArg)
{
    CPlayer_Chick* pInstance = new CPlayer_Chick(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CPlayer_Chick");
    }
    return pInstance;
}

void CPlayer_Chick::Free()
{
    __super::Free();
}
