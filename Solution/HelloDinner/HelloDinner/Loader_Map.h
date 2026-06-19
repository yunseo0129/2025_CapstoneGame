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

public:
    static CLoader_Map* Create(EngineContext* pContext);
    virtual void Free() override;
};