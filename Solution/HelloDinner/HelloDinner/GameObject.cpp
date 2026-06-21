#include "GameObject.h"
#include "GameInstance.h"

CGameObject::CGameObject(EngineContext* _pcontext)
	: m_pContext{ _pcontext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

CGameObject::CGameObject(const CGameObject& Prototype)
	: m_pContext{ Prototype.m_pContext }
	, m_pGameInstance{ Prototype.m_pGameInstance }
{
	Safe_AddRef(m_pGameInstance);
}

HRESULT CGameObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CGameObject::Initialize(void* pArg)
{
	if (nullptr != pArg)
	{
		GAMEOBJECT_DESC* pDesc = static_cast<GAMEOBJECT_DESC*>(pArg);
		m_iData = pDesc->iData;
	}

	m_pTransformCom = CTransform::Create(m_pContext);
	if (nullptr == m_pTransformCom)
		return E_FAIL;

	if (FAILED(m_pTransformCom->Initialize(pArg)))
		return E_FAIL;

	return S_OK;
}

void CGameObject::Priority_Update(_float fTimeDelta)
{
}

void CGameObject::Update(_float fTimeDelta)
{
}

void CGameObject::Late_Update(_float fTimeDelta)
{
}

void   CGameObject::TakeDamage(_uint _val)
{
    if (m_isDie)
        return;
    m_ihealth -= _val;
    if (m_ihealth <= 0)
    {
        Die(_val);
    }

    int ran = (int)m_pGameInstance->Compute_Random(1.f, 3.99f);
    _vector pos = m_pTransformCom->Get_State(CTransform::STATE_POSITION);
    _float3 posi;
    XMStoreFloat3(&posi, pos);
    string sound = "hit" + to_string(ran);
    m_pGameInstance->PlaySounds(sound);
    m_pGameInstance->UpdateSoundVolumeByPlayer(sound, posi);

}

void    CGameObject::Die(_float _val)
{
    m_isDie = true;
}

void CGameObject::Render(ID3D12GraphicsCommandList* _commandList)
{
}

void CGameObject::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
}

void CGameObject::Free()
{
	for (auto& Pair : m_Components) {
		Safe_Release(Pair.second);
	}
	m_Components.clear();

	Safe_Release(m_pTransformCom);
	Safe_Release(m_pGameInstance);

	__super::Free();
}

CComponent* CGameObject::Find_Component(const _wstring& strComponentTag)
{
	auto	iter = m_Components.find(strComponentTag);

	if (iter == m_Components.end())
		return nullptr;

	return iter->second;
}

const _float4x4* CGameObject::Get_WorldMatrix4x4Ptr()
{
	return m_pTransformCom->Get_WorldFloat4x4_Ptr();
}

HRESULT CGameObject::Add_Component(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, const _wstring& strComponentTag, CComponent** ppOut, void* pArg)
{
	if (nullptr != Find_Component(strComponentTag)) {
		MSG_BOX("Failed to Add_Component : Component with the same tag already exists.");
		return E_FAIL;
	}

	CComponent* pComponent = static_cast<CComponent*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_COMPONENT, iPrototypeLevelIndex, strPrototypeTag, pArg));
	if (nullptr == pComponent)
		return E_FAIL;

	m_Components.emplace(strComponentTag, pComponent);

	*ppOut = pComponent;

	Safe_AddRef(pComponent);

	return S_OK;
}

void CGameObject::Cull_And_Submit(CRenderer::RENDERGROUP eGroup)
{
    _bool bMain = true;
    _bool bShadow = true;

    _float3 vCenter; _float fRadius;
    if (Get_WorldBoundingSphere(vCenter, fRadius))
    {
        bMain = m_pGameInstance->IsSphereInFrustum(vCenter, fRadius);
        bShadow = m_pGameInstance->IsSphereInShadowFrustum(vCenter, fRadius);
    }

    m_pGameInstance->Add_CullStat_Main(bMain);
    m_pGameInstance->Add_CullStat_Shadow(bShadow);

    if (bMain)
        m_pGameInstance->Add_RenderObject(eGroup, this);
    if (bShadow)
        m_pGameInstance->Add_ShadowRenderObject(eGroup, this);
}
