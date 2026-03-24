#pragma once

// 레벨 로딩 클래스 변경하면 옮길 것들
#include "Base.h"

class CLoader_Map final : public CBase
{
private:
	CLoader_Map(ID3D12Device* pDevice, EngineContext* pContext);
	virtual ~CLoader_Map() = default;

public:
	HRESULT Load_MapData(const string& strJsonPath, _uint iLevelIndex);

private:
	// fbxName → 바이너리 파일 경로 변환
	_wstring	Get_BinaryPath(const string& strFbxName);
	// fbxName → 프로토타입 태그 생성
	_wstring	Get_ModelTag(const string& strFbxName);

private:
	ID3D12Device* m_pDevice = { nullptr };
	EngineContext* m_pContext = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };

public:
	static CLoader_Map* Create(ID3D12Device* pDevice, EngineContext* pContext);
	virtual void Free() override;
};
