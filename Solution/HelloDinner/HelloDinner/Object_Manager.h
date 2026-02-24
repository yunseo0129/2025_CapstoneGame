#pragma once

/* 레이어 : 내가 나누고 싶은 기준으로 나눠서 보관한 그룹. */
/* 레이어들을 레벨별로 보관한다. */

// Level -> Object_Manager -> Layer -> GameObject

#include "Base.h"

class CGameObject;

class CObject_Manager final : public CBase
{
private:
	CObject_Manager();
	virtual ~CObject_Manager() = default;

public:
	HRESULT Initialize(_uint iNumLevels);
	HRESULT Add_GameObject_ToLayer(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg = nullptr);
	CGameObject* Add_GameObject_ToLayer_Return_Obj(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg = nullptr);
	void Priority_Update(_float fTimeDelta);
	void Update(_float fTimeDelta);
	void Late_Update(_float fTimeDelta);
	void Clear(_uint iLevelIndex);
	CGameObject* Get_GameObject_To_Layer(_uint iLevelIndex, const _wstring& strLayerTag, _uint Index);

	list<class CGameObject*> Get_List(_uint iLevelIndex, const _wstring& strLayerTag);

private:
	_uint										m_iNumLevels = { 0 };
	map<const _wstring, class CLayer*>* m_pLayers = { nullptr };
	typedef map<const _wstring, class CLayer*>	LAYERS;

	class CGameInstance* m_pGameInstance = { nullptr };

private:
	class CLayer* Find_Layer(_uint iLevelIndex, const _wstring& strLayerTag);

public:
	static CObject_Manager* Create(_uint iNumLevels);
	virtual void Free() override;
};