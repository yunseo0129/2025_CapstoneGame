#include "Load_Manager.h"

CLoad_Manager::CLoad_Manager()
{
}

HRESULT CLoad_Manager::Initialize()
{
	return S_OK;
}

HRESULT CLoad_Manager::Open_File(const wchar_t* filename)
{
	m_hFile = CreateFile(filename,		// 파일 경로와 이름을 명시
		GENERIC_READ,			// 파일 접근 모드(GENERIC_WRITE : 쓰기 전용, GENERIC_READ : 읽기 전용)
		FILE_SHARE_READ,		// 읽기 전용 공유: 여러 프로세스(멀티 클라이언트)가 동시에 같은 파일을 열 수 있음
		NULL,					// 보안 속성, NULL인 기본 값 설정 유지
		OPEN_EXISTING,			// 생성 방식( OPEN_EXISTING : 파일이 있을 경우에만 불러오기)
		FILE_ATTRIBUTE_NORMAL,	// 파일 속성( FILE_ATTRIBUTE_NORMAL : 아무런 속성이 없는 파일)
		NULL);					// 생성될 파일의 속성을 제공할 템플릿 파일(사용하지 않기 때문에 NULL)

	if (INVALID_HANDLE_VALUE == m_hFile)
	{
		MSG_BOX("Load Failed");
		return E_FAIL;
	}

	return S_OK;
}

CLoad_Manager* CLoad_Manager::Create()
{
	CLoad_Manager* pInstance = new CLoad_Manager();

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CLoad_Manager");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CLoad_Manager::Free()
{
	__super::Free();
}

