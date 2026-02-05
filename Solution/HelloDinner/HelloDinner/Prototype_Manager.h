#pragma once
#include "Base.h"

class CPrototype_Manager final : public CBase
{
private:
	CPrototype_Manager();
	virtual ~CPrototype_Manager() = default;

public:
	HRESULT Initialize(_uint iNumLevels);
	HRESULT Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, class CBase* pPrototype);
	class CBase* Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg);
	void Clear(_uint iLevelIndex);
private:
	_uint										m_iNumLevels = { 0 };
	map<const _wstring, class CBase*>* m_pPrototypes = { nullptr };
	typedef map<const _wstring, class CBase*>	PROTOTYPE;

private:
	class CBase* Find_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag);



public:
	static CPrototype_Manager* Create(_uint iNumLevels);
	virtual void Free();

};