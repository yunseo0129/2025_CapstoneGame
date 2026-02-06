#pragma once
#include "Prototype_Manager.h"
#include "Graphic_Device.h"

class CGameInstance final : public CBase
{
	DECLARE_SINGLETON(CGameInstance)
private:
	CGameInstance();
	virtual ~CGameInstance() = default;

public: /* For.GameInstance */
	HRESULT Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext);
	void Update_Engine(_float fTimeDelta);
	//HRESULT Render_Begin(const _float4& vClearColor = _float4(0.f, 0.f, 1.f, 1.f));
	//HRESULT Draw();
	//HRESULT Render_End();
	//void Clear(_int iLevelID);

	_float Compute_Random_Normal();
	_float Compute_Random(_float fMin, _float fMax);

public: /* For.Prototype_Manager */
	HRESULT Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, class CBase* pPrototype);
	class CBase* Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg = nullptr);

private:
	class CGraphic_Device* m_pGraphic_Device = { nullptr };
	class CPrototype_Manager* m_pPrototype_Manager = { nullptr };

public:
	static void Release_Engine();

public:
	virtual void Free() override;
};