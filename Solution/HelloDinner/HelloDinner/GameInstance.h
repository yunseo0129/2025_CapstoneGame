#pragma once
#include "Prototype_Manager.h"
#include "Graphic_Device.h"
#include "PipeLine.h"
#include "Input_Device.h"

class CGameInstance final : public CBase
{
	DECLARE_SINGLETON(CGameInstance)
private:
	CGameInstance();
	virtual ~CGameInstance() = default;

public: /* For.GameInstance */
	HRESULT Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext);
	void Update_Engine(_float fTimeDelta);
	HRESULT Render_Begin(const _float4& vClearColor = _float4(0.f, 1.f, 0.f, 0.f));
	HRESULT Draw();
	HRESULT Render_End();
	//void Clear(_int iLevelID);

	_float Compute_Random_Normal();
	_float Compute_Random(_float fMin, _float fMax);

public: /* For.Prototype_Manager */
	HRESULT Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, class CBase* pPrototype);
	class CBase* Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg = nullptr);

public: /* For.PipeLine */
	void Set_Transform(CPipeLine::D3DTRANSFORMSTATE eState, _fmatrix TransformMatrix);
	_matrix Get_ViewProjMatrix();
	_matrix Get_TransformMatrix(CPipeLine::D3DTRANSFORMSTATE eState);
	_float4x4 Get_TransformFloat4x4(CPipeLine::D3DTRANSFORMSTATE eState);
	const _float4*Get_CamPosition() const;

public: /* for.Input_Device */
	_byte Get_DIKeyState(_ubyte byKeyID);
	_byte Get_DIMouseState(Engine::MOUSEKEYSTATE eMouse);
	_long Get_DIMouseMove(Engine::MOUSEMOVESTATE eMouseState);
	bool Key_Pressing(int _iKey);
	bool Key_Down(int _iKey);
	bool Key_Up(int _iKey);
	bool Mouse_Pressing(int _iKey);
	bool Mouse_Down(int _iKey);
	bool Mouse_Up(int _iKey);

private:
	class CGraphic_Device*		m_pGraphic_Device = { nullptr };
	class CPrototype_Manager*	m_pPrototype_Manager = { nullptr };
	class CPipeLine*			m_pPipeLine = { nullptr };
	class CInput_Device*		m_pInput_Device = { nullptr };

public:
	static void Release_Engine();

public:
	virtual void Free() override;
};