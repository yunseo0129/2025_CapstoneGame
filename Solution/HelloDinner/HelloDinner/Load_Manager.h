#pragma once

/* 레이어 : 내가 나누고 싶은 기준으로 나눠서 보관한 그룹. */
/* 레이어들을 레벨별로 보관한다. */
#include "Base.h"

class CLoad_Manager final : public CBase
{
private:
	CLoad_Manager();
	virtual ~CLoad_Manager() = default;

public:
	HRESULT Initialize();

	HRESULT Open_File(const wchar_t* filename);
	void	Close_File() { CloseHandle(m_hFile); }

	void	Read_File(_char& read) { if (!ReadFile(m_hFile, &read, sizeof(_char), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _char"); } }
	void	Read_File(size_t& read) { if (!ReadFile(m_hFile, &read, sizeof(size_t), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File size_t"); } }
	void	Read_File(_int& read) { if (!ReadFile(m_hFile, &read, sizeof(_int), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _int"); } }
	void	Read_File(_uint& read) { if (!ReadFile(m_hFile, &read, sizeof(_uint), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _uint"); } }
	void	Read_File(_float& read) { if (!ReadFile(m_hFile, &read, sizeof(_float), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _float"); } }
	void	Read_File(_float2& read) { if (!ReadFile(m_hFile, &read, sizeof(_float2), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _float2"); } }
	void	Read_File(_float3& read) { if (!ReadFile(m_hFile, &read, sizeof(_float3), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _float3"); } }
	void	Read_File(_float4& read) { if (!ReadFile(m_hFile, &read, sizeof(_float4), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _float4"); } }
	void	Read_File(XMUINT4& read) { if (!ReadFile(m_hFile, &read, sizeof(XMUINT4), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File XMUINT4"); } }
	void	Read_File(_float4x4& read) { if (!ReadFile(m_hFile, &read, sizeof(_float4x4), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _float4x4"); } }
	void	Read_File(VTXMESH& read) { if (!ReadFile(m_hFile, &read, sizeof(VTXMESH), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File VTXMESH"); } }
	void	Read_File(VTXANIMMESH& read) { if (!ReadFile(m_hFile, &read, sizeof(VTXANIMMESH), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File VTXANIMMESH"); } }
	void	Read_File(KEYFRAME& read) { if (!ReadFile(m_hFile, &read, sizeof(KEYFRAME), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File KEYFRAME"); } }
	// 수정 필요함
	void	Read_File(char(&read)[MAX_PATH]) { if (!ReadFile(m_hFile, read, MAX_PATH * sizeof(char), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File char[MAX_PATH]"); } }
	void	Read_File(_tchar(&read)[MAX_PATH]) { if (!ReadFile(m_hFile, read, MAX_PATH * sizeof(_tchar), &m_dwByte, nullptr)) { MSG_BOX("Failed to Load File _tchar[MAX_PATH]"); } }

private:
	HANDLE		m_hFile = {};
	DWORD		m_dwByte = {};

public:
	static CLoad_Manager* Create();
	virtual void Free() override;
};
