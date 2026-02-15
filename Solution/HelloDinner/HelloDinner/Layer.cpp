#include "Layer.h"
#include "GameObject.h"

CLayer::CLayer()
{
}

HRESULT CLayer::Add_GameObject(CGameObject* pGameObject)
{
	if (nullptr == pGameObject)
		return E_FAIL;

	m_GameObjects.push_back(pGameObject);

	return S_OK;
}

CGameObject* CLayer::Get_GameObject(_uint iNum)
{
	_uint i = 0;
	for (auto GameObject : m_GameObjects)
	{
		if (i == iNum)
			return GameObject;

		++i;
	}
	return nullptr;
}

void CLayer::Priority_Update(_float fTimeDelta)
{
	for (auto it = m_GameObjects.begin(); it != m_GameObjects.end();)
	{
		if ((*it)->IsDead())
		{
			Safe_Release(*it);
			it = m_GameObjects.erase(it);  // 'erase'는 지운 요소 다음 요소의 반복자를 반환
		}
		else
		{
			(*it)->Priority_Update(fTimeDelta);
			++it;  // 죽지 않은 오브젝트의 경우만 반복자 증가
		}
	}
}

void CLayer::Update(_float fTimeDelta)
{
	for (auto& pGameObject : m_GameObjects)
		pGameObject->Update(fTimeDelta);
}

void CLayer::Late_Update(_float fTimeDelta)
{
	for (auto& pGameObject : m_GameObjects)
		pGameObject->Late_Update(fTimeDelta);
}

CLayer* CLayer::Create()
{
	return new CLayer();
}


void CLayer::Free()
{
	__super::Free();

	for (auto& pGameObject : m_GameObjects)
		Safe_Release(pGameObject);

	m_GameObjects.clear();
}
