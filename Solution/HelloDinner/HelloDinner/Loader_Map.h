#pragma once

// 레벨 로딩 클래스 변경하면 옮길 것들
#include "Base.h"

class CLoader_Map final: public CBase
{
private:
    struct MATERIAL_INFO
    {
        _wstring strAlbedoFile {};
        _wstring strNormalFile {};

        // [팔레트 방식] 이 슬롯이 어느 공유 팔레트를 쓰는지 고르기 위한 Unity 머티리얼 이름.
        //   대부분 'Kitchen'/'Gold'/'Glass' 등 → 공용 Palette 한 장으로 충분(메시 UV 가
        //   팔레트의 올바른 색을 직접 가리킴). 'Paintings' 만 별도 Paintings 팔레트 사용.
        string   strMaterialName {};

        // [방식 가 - 레거시] 팔레트 crop UV 재매핑: finalUV = meshUV * uvScale + uvOffset.
        //   팔레트 직접 매핑으로 전환하면 메시 원본 UV 를 그대로 쓰므로 사실상 미사용
        //   (기본값 0,0 / 1,1 = 변환 없음). 호환을 위해 필드는 유지.
        _float2  vUVOffset {0.f, 0.f};
        _float2  vUVScale {1.f, 1.f};
    };
private:
    CLoader_Map(EngineContext* pContext);
    virtual ~CLoader_Map() = default;

public:
    // MaterialData.json을 읽어 Material 정보 테이블 구축
    HRESULT Load_MaterialData(const string& strJsonPath);

    // MapData.json을 읽어 모델 + 맵 오브젝트 생성
    HRESULT Load_MapData(const string& strJsonPath, _uint iLevelIndex);

    HRESULT Check_Fbx_Existence(const string& strJsonPath);

private:
    // fbxName → 바이너리 파일 경로 변환
    _wstring	Get_BinaryPath(const string& strFbxName, bool bAnim);
    // fbxName → 프로토타입 태그 생성
    _wstring	Get_ModelTag(const string& strFbxName);
    // PNG 파일명 → 텍스처 Prototype 태그 변환
    _wstring	Get_TextureTag(const string& strPngFileName);

private:
    // GPU 커맨드 플러시(배치 업로드용)
    // 후에 CopyQueue 제작하면 삭제
    void		FlushCommandList();
    // 플러시 후 대기 중인 리소스들을 안전하게 해제
    void		ReleasePendingResources();
    _uint m_iLoadCounter = {0};
    static const _uint FLUSH_INTERVAL = 5; // 10개마다 GPU 플러시
    // 플러시 전까지 삭제를 보류할 리소스들
    vector<class CComponent*> m_PendingReleases;
    //
private:
    EngineContext* m_pContext = {nullptr};
    class CGameInstance* m_pGameInstance = {nullptr};

    // material file을 읽어 존재하는 texture 파일명을 저장
    unordered_map<std::string, MATERIAL_INFO> m_MaterialInfos;

    // 텍스처(dds) 공통 경로 프리픽스.
    const _wstring m_strTextureDir = L"Resources/NonAnim/Map/dds/";

    // [팔레트 방식] 공유 팔레트 DDS 경로.
    //   맵의 거의 모든 머티리얼은 단일 Palette 를 공유하고, 메시 UV 가 팔레트의
    //   올바른 영역을 직접 가리킨다. 그림 액자(Picture_Assembly_*, Photo_*)의
    //   'Paintings' 슬롯만 별도 Paintings 팔레트를 사용한다.
    //   ※ Unity 에서 Palette.png / Paintings.png 를 dds 로 변환해 이 경로에 둘 것.
    const _wstring m_strPaletteDefault = m_strTextureDir + L"Palette.dds";
    const _wstring m_strPalettePaintings = m_strTextureDir + L"Paintings.dds";

public:
    static CLoader_Map* Create(EngineContext* pContext);
    virtual void Free() override;
};