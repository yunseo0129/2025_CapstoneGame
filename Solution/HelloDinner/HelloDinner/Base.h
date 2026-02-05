#pragma once

#include "stdafx.h"

class CBase abstract
{
protected:
	CBase();
	virtual ~CBase() = default;

public:
	_uint AddRef();
	_uint Release();

private:
	_uint			m_iRefCnt = { 0 };

public:
	virtual void Free();
};

template<typename T>
void	Safe_Delete(T& Pointer)
{
	if (nullptr != Pointer)
	{
		delete Pointer;
		Pointer = nullptr;
	}
}

template<typename T>
void	Safe_Delete_Array(T& Pointer)
{
	if (nullptr != Pointer)
	{
		delete[] Pointer;
		Pointer = nullptr;
	}
}

template<typename T>
unsigned int Safe_AddRef(T& pInstance)
{
	unsigned int		iRefCnt = 0;

	if (nullptr != pInstance)
		iRefCnt = pInstance->AddRef();

	return iRefCnt;
}

template<typename T>
unsigned int Safe_Release(T& pInstance)
{
	unsigned int		iRefCnt = 0;

	if (nullptr != pInstance)
	{
		iRefCnt = pInstance->Release();

		if (0 == iRefCnt)
			pInstance = nullptr;
	}

	return iRefCnt;
}