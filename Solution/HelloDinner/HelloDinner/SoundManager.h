#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <WinSock2.h>
#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <cmath>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

#include "Base.h"

class SoundManager final : public CBase {
public:
    SoundManager();
    ~SoundManager();

    bool Initialize(HWND hwnd);
    void Shutdown();

    bool LoadSound(const std::string& soundName, const std::string& filename);
    void PlaySound(const std::string& soundName, bool loop = false);
    void StopSound(const std::string& soundName);

    void SetSoundVolume(const std::string& soundName, float volume); // 0.0f (mute) to 1.0f (max)
    void UpdateSoundVolumeByDistance(const std::string& soundName, float listenerX, float listenerY, float listenerZ, float soundX, float soundY, float soundZ);
    void UpdateSoundVolumeByPlayer(const std::string& soundName, _float3 soundpos);

    bool IsSoundPlaying(const std::string& soundName);

    void UpdateBGM();

    void Set_PlayerPos(_float3* pos) { m_pPlayerPos = pos; }

private:
    bool InitializeDirectSound(HWND hwnd);
    void ShutdownDirectSound();

    bool LoadOggFile(const std::string& filename, IDirectSoundBuffer8** buffer);
    void ReleaseOggFile(IDirectSoundBuffer8** buffer);

private:
    IDirectSound8* m_directSound;
    IDirectSoundBuffer* m_primaryBuffer;

    std::unordered_map<std::string, IDirectSoundBuffer8*> m_soundBuffers;
    _float3* m_pPlayerPos = { nullptr };
};