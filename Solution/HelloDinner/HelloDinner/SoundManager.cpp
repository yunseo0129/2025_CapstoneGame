#include "SoundManager.h"
#include "stb_vorbis.c"

SoundManager::SoundManager() : m_directSound(nullptr), m_primaryBuffer(nullptr) {}

SoundManager::~SoundManager() {
    Shutdown();
}

bool SoundManager::Initialize(HWND hwnd) {
    return InitializeDirectSound(hwnd);
}

void SoundManager::Shutdown() {
    for (auto& pair : m_soundBuffers) {
        ReleaseOggFile(&pair.second);
    }
    ShutdownDirectSound();
}

bool SoundManager::LoadSound(const std::string& soundName, const std::string& filename) {
    IDirectSoundBuffer8* buffer = nullptr;
    if (!LoadOggFile(filename, &buffer)) {
        return false;
    }
    m_soundBuffers[soundName] = buffer;
    return true;
}

void SoundManager::PlaySound(const std::string& soundName, bool loop) {
    auto it = m_soundBuffers.find(soundName);
    if (it != m_soundBuffers.end()) {
        DWORD loopFlag = loop ? DSBPLAY_LOOPING : 0;
        it->second->SetCurrentPosition(0);
        it->second->Play(0, 0, loopFlag);
    }
}

void SoundManager::StopSound(const std::string& soundName) {
    auto it = m_soundBuffers.find(soundName);
    if (it != m_soundBuffers.end()) {
        it->second->Stop();
    }
}

void SoundManager::SetSoundVolume(const std::string& soundName, float volume) {
    auto it = m_soundBuffers.find(soundName);
    if (it != m_soundBuffers.end()) {
        // Volume in DirectSound ranges from DSBVOLUME_MIN (-10000) to 0 (max)
        LONG dsVolume = static_cast<LONG>((volume * 10000.0f) - 10000.0f);
        it->second->SetVolume(dsVolume);
    }
}

void SoundManager::UpdateSoundVolumeByDistance(const std::string& soundName, float listenerX, float listenerY, float listenerZ, float soundX, float soundY, float soundZ) {
    float dx = soundX - listenerX;
    float dy = soundY - listenerY;
    float dz = soundZ - listenerZ;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    
    // Define max distance and min volume behavior
    const float maxDistance = 32.0f;
    float volume = max(0.0f, 1.0f - (distance / maxDistance));

    SetSoundVolume(soundName, volume);
}

void SoundManager::UpdateSoundVolumeByPlayer(const std::string& soundName, _float3 soundpos)
{
    float dx = soundpos.x - m_pPlayerPos->x;
    float dy = soundpos.y - m_pPlayerPos->y;
    float dz = soundpos.z - m_pPlayerPos->z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Define max distance and min volume behavior
    const float maxDistance = 32.0f;
    float volume = max(0.0f, 1.0f - (distance / maxDistance));

    SetSoundVolume(soundName, volume);
}

bool SoundManager::IsSoundPlaying(const std::string& soundName) {
    auto it = m_soundBuffers.find(soundName);
    if (it != m_soundBuffers.end()) {
        DWORD status;
        if (SUCCEEDED(it->second->GetStatus(&status))) {
            return (status & DSBSTATUS_PLAYING) != 0;
        }
    }
    return false;
}

void SoundManager::UpdateBGM() {
    static std::string currentBGM = "bgm1"; // 현재 재생 중인 BGM 이름
    static std::string nextBGM = "bgm2";   // 다음에 재생할 BGM 이름
    static std::string BGM3 = "bgm3";   // 다음에 재생할 BGM 이름
    static std::string BGM4 = "bgm4";   // 다음에 재생할 BGM 이름
    static std::string BGM5 = "bgm5";   // 다음에 재생할 BGM 이름
    static std::string BGM6 = "bgm6";   // 다음에 재생할 BGM 이름
    static std::string BGM7 = "bgm7";   // 다음에 재생할 BGM 이름

    if (!IsSoundPlaying(currentBGM)) {
        // 현재 BGM이 끝났으면 다음 BGM 재생
        PlaySound(nextBGM); 
        SetSoundVolume(nextBGM, 0.7f);
        static std::string tmp = currentBGM;
        currentBGM = nextBGM;
        nextBGM = BGM3;
        BGM3 = BGM4;
        BGM4 = BGM5;
        BGM5 = BGM6;
        BGM6 = BGM7;
        BGM7 = tmp;
    }
}

bool SoundManager::InitializeDirectSound(HWND hwnd) {
    HRESULT result;

    result = DirectSoundCreate8(nullptr, &m_directSound, nullptr);
    if (FAILED(result)) {
        return false;
    }

    result = m_directSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
    if (FAILED(result)) {
        return false;
    }

    DSBUFFERDESC bufferDesc = {};
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    result = m_directSound->CreateSoundBuffer(&bufferDesc, &m_primaryBuffer, nullptr);
    if (FAILED(result)) {
        return false;
    }

    return true;
}

void SoundManager::ShutdownDirectSound() {
    if (m_primaryBuffer) {
        m_primaryBuffer->Release();
        m_primaryBuffer = nullptr;
    }

    if (m_directSound) {
        m_directSound->Release();
        m_directSound = nullptr;
    }
}

bool SoundManager::LoadOggFile(const std::string& filename, IDirectSoundBuffer8** buffer) {
    int channels, sampleRate;
    short* decoded;
    int samples = stb_vorbis_decode_filename(filename.c_str(), &channels, &sampleRate, &decoded);
    if (samples < 0) {
        return false;
    }

    // Create DirectSound buffer description
    DSBUFFERDESC bufferDesc = {};
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME;
    bufferDesc.dwBufferBytes = samples * sizeof(short) * channels;
    bufferDesc.lpwfxFormat = new WAVEFORMATEX;
    bufferDesc.lpwfxFormat->wFormatTag = WAVE_FORMAT_PCM;
    bufferDesc.lpwfxFormat->nChannels = channels;
    bufferDesc.lpwfxFormat->nSamplesPerSec = sampleRate;
    bufferDesc.lpwfxFormat->wBitsPerSample = 16;
    bufferDesc.lpwfxFormat->nBlockAlign = (channels * bufferDesc.lpwfxFormat->wBitsPerSample) / 8;
    bufferDesc.lpwfxFormat->nAvgBytesPerSec = bufferDesc.lpwfxFormat->nSamplesPerSec * bufferDesc.lpwfxFormat->nBlockAlign;

    // Create buffer
    IDirectSoundBuffer* tempBuffer;
    if (FAILED(m_directSound->CreateSoundBuffer(&bufferDesc, &tempBuffer, nullptr))) {
        delete[] decoded;
        delete bufferDesc.lpwfxFormat;
        return false;
    }

    // Query for the secondary buffer
    if (FAILED(tempBuffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)buffer))) {
        tempBuffer->Release();
        delete[] decoded;
        delete bufferDesc.lpwfxFormat;
        return false;
    }

    tempBuffer->Release();

    // Lock the buffer
    void* bufferPtr;
    DWORD bufferSize;
    if (FAILED((*buffer)->Lock(0, bufferDesc.dwBufferBytes, &bufferPtr, &bufferSize, nullptr, nullptr, 0))) {
        (*buffer)->Release();
        delete[] decoded;
        delete bufferDesc.lpwfxFormat;
        return false;
    }

    // Copy PCM data to buffer
    memcpy(bufferPtr, decoded, bufferSize);

    // Unlock and clean up
    (*buffer)->Unlock(bufferPtr, bufferSize, nullptr, 0);
    delete[] decoded;
    delete bufferDesc.lpwfxFormat;

    return true;
}

void SoundManager::ReleaseOggFile(IDirectSoundBuffer8** buffer) {
    if (*buffer) {
        (*buffer)->Release();
        *buffer = nullptr;
    }
}
