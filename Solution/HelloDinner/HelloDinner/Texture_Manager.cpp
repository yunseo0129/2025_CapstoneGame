#include "DefaultTexture_Manager.h"
#include "Texture.h"

CDefaultTexture_Manager::CDefaultTexture_Manager(EngineContext* _context)
: m_pContext{ _context }
{
}

HRESULT CDefaultTexture_Manager::Initialize()
{
	// 기본 노말 텍스쳐 생성
	// 1x1 하늘색? 텍스쳐
	m_pDefaultNormalTexture = CTexture::Create(m_pContext, L"Resources/Textures/DefaultNormal.png", 1);


	return S_OK;
}



CDefaultTexture_Manager* CDefaultTexture_Manager::Create(EngineContext* _context)
{
	CDefaultTexture_Manager* pInstance = new CDefaultTexture_Manager(_context);
	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Create : CDefaultTexture_Manager");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CDefaultTexture_Manager::Free()
{
	__super::Free();
	Safe_Release(m_pDefaultNormalTexture);
}