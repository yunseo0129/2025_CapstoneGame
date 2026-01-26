#pragma once
#include "stdafx.h"
#include "shader.h"
#include "mesh.h"
#include "object.h"
#include "player.h"
#include "camera.h"
#include "texture.h"
class CScene 
{
public:
	CScene();
	~CScene() = default;

	void MouseEvent(HWND _hWnd, FLOAT _timeElapsed);
	void KeyboardEvent(FLOAT _timeElapsed);
	void Update(FLOAT _timeElapsed);
	void Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const;

	void BuildObjects(const ComPtr<ID3D12Device>& _device,
		const ComPtr<ID3D12GraphicsCommandList>& _commandList,
		const ComPtr<ID3D12RootSignature>& _rootSignature);
	void ReleaseUploadBuffer();

	void MouseEvent(UINT _message, LPARAM _lParam);
	void KeyboardEvent(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam);

private:
	unordered_map<string, shared_ptr<CShader>> m_mapShaders;
	unordered_map<string, shared_ptr<CMesh>> m_mapMeshes;
	unordered_map<string, shared_ptr<CTexture>> m_mapTextures;

	shared_ptr<Camera> m_pCamera;
	shared_ptr<CPlayer> m_pPlayer;
	vector<shared_ptr<CGameObject>> m_vecObjects;
	shared_ptr<CGameObject> m_pSkybox;

};