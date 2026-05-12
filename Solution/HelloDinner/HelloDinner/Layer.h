#pragma once

#include "Base.h"

/* 객체들을 레이어 별로 묶어서 다수 보관한다. */

class CLayer final : public CBase
{
private:
	CLayer();
	virtual ~CLayer() = default;

public:
	HRESULT Add_GameObject(class CGameObject* pGameObject);
	CGameObject* Get_GameObject(_uint iNum);
	void Priority_Update(_float fTimeDelta);
	void Update(_float fTimeDelta);
	void Late_Update(_float fTimeDelta);
	list<class CGameObject*> Get_List() { return m_GameObjects; }

private:
	// 추후 std::map으로 레이어 내부에서 PSO별로 나누기
	// why? PSO가 다르면 Render()에서 SetPipelineState()로 PSO를 바꿔줘야 하는데,
	// 이게 비용이 크다.
	list<class CGameObject*>			m_GameObjects;

public:
	static CLayer* Create();
	virtual void Free() override;
};