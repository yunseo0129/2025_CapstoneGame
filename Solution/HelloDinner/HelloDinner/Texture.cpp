#include "Texture.h"
#include "GameInstance.h"
#include "../HelloDinner/Common/DDSTextureLoader12.h"
#include "../HelloDinner/Common/WICTextureLoader12.h"

CTexture::CTexture(EngineContext* _pDevice)
    : CComponent(_pDevice)
{
}

CTexture::CTexture(const CTexture& Prototype)
    : CComponent(Prototype.m_pContext)
    , m_iNumTextures(Prototype.m_iNumTextures)
    , m_Textures(Prototype.m_Textures)                // 리소스 공유 (ComPtr로 레퍼런스 카운트 증가)
	, m_iSRVIndex(Prototype.m_iSRVIndex)
{
    for (auto& pTexture : m_Textures)
		Safe_AddRef(pTexture); // 텍스처 리소스 참조 카운트 증가
    // Clone된 객체는 UploadBuffer를 가질 필요가 없으므로 복사하지 않음
}

HRESULT CTexture::Initialize_Prototype(ID3D12GraphicsCommandList* _cmdList, const _tchar* pTextureFilePath, _uint iNumTexture, TEXTURE_TYPE _iTextureType )
{
	// 1. 텍스처 수 저장
    m_iNumTextures = iNumTexture;
	// 2. 텍스처 로딩 및 SRV 생성 루프
    m_Textures.resize(m_iNumTextures);
    m_UploadBuffers.resize(m_iNumTextures); // 업로드 버퍼 보관

    // 힙의 시작 핸들 가져오기
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor = m_pGameInstance->Get_CPUHandle();

    _tchar			szTextureFilePath[MAX_PATH] = TEXT("");

    for (_uint i = 0; i < m_iNumTextures; ++i)
    {
        // 포맷 문자열(%d 등)이 포함된 경로인지 확인
        if (wcschr(pTextureFilePath, L'%') != nullptr)
            wsprintf(szTextureFilePath, pTextureFilePath, i);
        else
            wcscpy_s(szTextureFilePath, pTextureFilePath);

        // 빈 경로 스킵
        if (wcslen(szTextureFilePath) == 0)
            continue;

        /* 드라이브경로, 디렉토리경로, 파일네임, 파일확장자. */
        _tchar		szEXT[MAX_PATH] = TEXT("");

        /* D:\정의훈\147\3d\Framework\Client\Bin\Resources\Textures\Default.jpg */
        _wsplitpath_s(szTextureFilePath, nullptr, 0, nullptr, 0, nullptr, 0, szEXT, MAX_PATH);

        // 확인
        char szLog[1024];
        char szPathA[MAX_PATH];
        char szExtA[MAX_PATH];

        // 3. 텍스처 파일 로드
        if (false == lstrcmpW(szEXT, TEXT(".dds")))
        {
            if (FAILED(Load_DDSTexture(_cmdList, szTextureFilePath, i)))
                return E_FAIL;
        }
        else if (false == lstrcmpW(szEXT, TEXT(".png")) ||
            false == lstrcmpW(szEXT, TEXT(".jpg")) ||
            false == lstrcmpW(szEXT, TEXT(".bmp")) ||
            false == lstrcmpW(szEXT, TEXT(".tiff")))
        {
            if (FAILED(Load_WICTexture(_cmdList, szTextureFilePath, i)))
                return E_FAIL;
        }
        else if (false == lstrcmpW(szEXT, TEXT(".psd")) ||
            false == lstrcmpW(szEXT, TEXT(".tga")))
        {
            // .psd와 .tga는 WIC 미지원
            continue;
        }
        else
        {
            MSG_BOX("Failed to Load : CTexture - Unsupported format");
            return E_FAIL;
        }
  

		// 4. 텍스처의 리소스 설명자 정보 저장
        if (FAILED(SetResourceDesc(m_Textures[i].Get(), _iTextureType)))
			return E_FAIL;

        // 5. Shader Resource View (SRV) 생성
        if (FAILED(CreateShaderResourceView(hDescriptor, i, _iTextureType )))
            return E_FAIL;

		m_iSRVIndex = m_pGameInstance->Get_CurrentIndex(); // 글로벌 SRV 힙에서의 시작 인덱스 저장 (Manager에서 할당받은 위치)

        // 다음 텍스처를 위해 핸들 오프셋 이동
		m_pGameInstance->Offset_DescriptorHandle(1);
    }

    return S_OK;
}

HRESULT CTexture::Initialize(void* pArg)
{
    // Clone된 객체의 초기화
    // 텍스처는 이미 Prototype에서 로드되었으므로 별도 작업 불필요
    return S_OK;
}

HRESULT CTexture::Bind_ShaderResource(ID3D12GraphicsCommandList* pCommandList, RootParameterIndex _eRootParameterIndex, _uint iTextureIndex)
{
    if (iTextureIndex >= m_iNumTextures)
        return E_FAIL;

    // 1. GPU 핸들 계산
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuHandle = m_pGameInstance->Get_GPUHandle(m_iSRVIndex); // 글로벌 SRV 힙에서의 시작 핸들 가져오기

    // 3. 루트 테이블에 바인딩
    pCommandList->SetGraphicsRootDescriptorTable( (_uint)_eRootParameterIndex , hGpuHandle);
    


    return S_OK;
}

void CTexture::Release_UploadBuffer()
{
    // GPU 전송이 확실히 끝난 후 호출해야 함 (Fence 동기화 이후)
    for (auto& buffer : m_UploadBuffers)
    {
        buffer.Reset(); // ComPtr 해제
    }
    m_UploadBuffers.clear();
}

HRESULT CTexture::Load_DDSTexture(ID3D12GraphicsCommandList* pCommandList, const wstring& _pFilePath, _uint _iIndex)
{
    // DDSTextureLoader 사용
    unique_ptr<uint8_t[]> ddsData;  // LoadDDSTextureFromFileEx 함수 내부에서 unique_ptr<uint8_t[]>을 인수로 받음
    vector<D3D12_SUBRESOURCE_DATA> subresources;
    DDS_ALPHA_MODE ddsAlphaMode{ DDS_ALPHA_MODE_UNKNOWN };
    if (FAILED(DirectX::LoadDDSTextureFromFileEx(m_pContext->device, _pFilePath.c_str(), 0,
        D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT, &m_Textures[_iIndex], ddsData, subresources, &ddsAlphaMode)))
        return E_FAIL;

    UINT nSubresources{ (UINT)subresources.size() };
    const UINT64 TextureSize{ GetRequiredIntermediateSize(m_Textures[_iIndex].Get(), 0, nSubresources)};

    // UploadBuffer 생성
    D3D12_HEAP_PROPERTIES heapUploadProps;
    SetHeapProperties(heapUploadProps, D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RESOURCE_DESC resourceDesc;
    SetResourceDesc(resourceDesc, TextureSize);

    if (FAILED(m_pContext->device->CreateCommittedResource(
        &heapUploadProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_UploadBuffers[_iIndex])
    )))
        return E_FAIL;

    UpdateSubresources(pCommandList, m_Textures[_iIndex].Get(), m_UploadBuffers[_iIndex].Get(), 0, 0, nSubresources, subresources.data());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_Textures[_iIndex].Get(),
        D3D12_RESOURCE_STATE_COPY_DEST , D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
    pCommandList->ResourceBarrier(1, &barrier);

    return S_OK;
}

HRESULT CTexture::Load_WICTexture(ID3D12GraphicsCommandList* pCommandList, const wstring& _pFilePath, _uint _iIndex)
{
    D3D12_SUBRESOURCE_DATA subresource{};
    unique_ptr<uint8_t[]> wicData;

    if (FAILED(DirectX::LoadWICTextureFromFile(m_pContext->device, _pFilePath.c_str(),
        &m_Textures[_iIndex], wicData, subresource)))
        return E_FAIL;

    const UINT64 TextureSize = GetRequiredIntermediateSize(m_Textures[_iIndex].Get(), 0, 1);

    // UploadBuffer 생성
    D3D12_HEAP_PROPERTIES heapUploadProps;
    SetHeapProperties(heapUploadProps, D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RESOURCE_DESC resourceDesc;
    SetResourceDesc(resourceDesc, TextureSize);

    if (FAILED(m_pContext->device->CreateCommittedResource(
        &heapUploadProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_UploadBuffers[_iIndex]))))
        return E_FAIL;

    UpdateSubresources(pCommandList, m_Textures[_iIndex].Get(), m_UploadBuffers[_iIndex].Get(),
        0, 0, 1, &subresource);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_Textures[_iIndex].Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pCommandList->ResourceBarrier(1, &barrier);

    return S_OK;
}

HRESULT CTexture::SetResourceDesc(ID3D12Resource* _pTexture, TEXTURE_TYPE _iTextureType ) {
    if (!_pTexture)
		return E_FAIL;
    
    D3D12_RESOURCE_DESC desc = _pTexture->GetDesc();
    m_FormatDesc = desc.Format;
    m_iWidth = desc.Width;
    m_iHeight = desc.Height; 
    m_iArraySize = desc.DepthOrArraySize;
    m_iMipLevels = desc.MipLevels;
    m_eType = _iTextureType;

	return S_OK;
}


HRESULT CTexture::CreateShaderResourceView(CD3DX12_CPU_DESCRIPTOR_HANDLE _descriptorHandle, _uint _iIndex, TEXTURE_TYPE _iTextureType )
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_Textures[_iIndex]->GetDesc().Format;

    switch ( _iTextureType )
    {
    case TEXTURE_TYPE::TEX_2D:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = m_Textures[_iIndex]->GetDesc().MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        break;
    case TEXTURE_TYPE::TEX_CUBE:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = m_Textures[_iIndex]->GetDesc().MipLevels;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        break;
    case TEXTURE_TYPE::TEX_ARRAY:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.ArraySize = m_iArraySize;
        break;
        break;
        // 필요시 계속 추가
    default:
        break;
    }
    m_pContext->device->CreateShaderResourceView(m_Textures[_iIndex].Get(), &srvDesc, _descriptorHandle);

    return S_OK;
}

void CTexture::SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type)
{
    _heapProps.Type = _type;
    _heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    _heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    _heapProps.CreationNodeMask = 1;
    _heapProps.VisibleNodeMask = 1;
}

void CTexture::SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _size)
{
    _resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    _resourceDesc.Alignment = 0;
    _resourceDesc.Width = _size;
    _resourceDesc.Height = 1;
    _resourceDesc.DepthOrArraySize = 1;
    _resourceDesc.MipLevels = 1;
    _resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    _resourceDesc.SampleDesc.Count = 1;
    _resourceDesc.SampleDesc.Quality = 0;
    _resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    _resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
}



CTexture* CTexture::Create(EngineContext* _pContext , const _tchar* cFilePath , _uint iNumTextures , TEXTURE_TYPE _iTextureType)
{
    CTexture* pInstance = new CTexture(_pContext);

    if (FAILED(pInstance->Initialize_Prototype(_pContext->cmdList, cFilePath , iNumTextures , _iTextureType )))
    {
        Safe_Release(pInstance); // 혹은 delete pInstance;
    }

    return pInstance;
}

CComponent* CTexture::Clone(void* pArg)
{
    CTexture* pInstance = new CTexture(*this); // 복사 생성자 호출

    if (FAILED(pInstance->Initialize(pArg)))
    {
        // MSG_BOX("Failed to Cloned : CTexture");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CTexture::Free()
{
    for (auto& texture : m_Textures)
    {
		texture.Reset(); // ComPtr 해제
	}

	__super::Free();
}