#include "Bounding.h"

CBounding::CBounding(EngineContext* pContext)
	: m_pContext{ pContext }
{
}

HRESULT CBounding::Initialize()
{
	return S_OK;
}

void CBounding::Free()
{
	__super::Free();
}