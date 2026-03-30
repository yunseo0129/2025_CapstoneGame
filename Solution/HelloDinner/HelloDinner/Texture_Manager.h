#pragma once

// Normal 텍스쳐 같은 NULL일 수 있는 텍스쳐의 기본값을 가지고 있는 클래스
#include "Base.h"

class CDefaultTexture_Manager final : public CBase
{
private:
	CDefaultTexture_Manager(EngineContext* _context);
	virtual ~CDefaultTexture_Manager() = default;

public:
	HRESULT Initialize();

	class CTexture* Get_DefaultNormalTexture() const { return m_pDefaultNormalTexture; }


private:
	EngineContext* m_pContext = { nullptr };

	class CTexture* m_pDefaultNormalTexture = { nullptr };

public:
	static CDefaultTexture_Manager* Create(EngineContext* _context);
	virtual void Free() override;
};
