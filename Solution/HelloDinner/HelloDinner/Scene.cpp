#include "scene.h"
#include "stdafx.h"
#include "MainApp.h"

CScene::CScene()
{
}

void CScene::MouseEvent(HWND _hWnd, FLOAT _timeElapsed)
{
	SetCursor(NULL);
	RECT windowRect;
	GetWindowRect(_hWnd, &windowRect);

	POINT lastMousePosition{
		windowRect.left + static_cast<LONG>(1280 / 2),
		windowRect.top + static_cast<LONG>(720 / 2) };
	POINT mousePosition;
	GetCursorPos(&mousePosition);

	float dx = XMConvertToRadians(0.15f * static_cast<FLOAT>(mousePosition.x - lastMousePosition.x));
	float dy = XMConvertToRadians(0.15f * static_cast<FLOAT>(mousePosition.y - lastMousePosition.y));

	if (m_pCamera) {
		m_pCamera->RotateYaw(-dx);
		m_pCamera->RotatePitch(dy);
	}
	SetCursorPos(lastMousePosition.x, lastMousePosition.y);

	m_pPlayer->MouseEvent(_timeElapsed);
}

void CScene::KeyboardEvent(FLOAT _timeElapsed)
{
	m_pPlayer->KeyboardEvent(_timeElapsed);
}

void CScene::Update(FLOAT _timeElapsed)
{
	m_pPlayer->Update(_timeElapsed);
	for (auto& object : m_vecObjects) {
		object->Update(_timeElapsed);
	}
	m_pSkybox->SetPosition(m_pCamera->GetEye());
}

void CScene::Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	m_pCamera->UpdateShaderVariable(_commandList);

	m_mapShaders.at("OBJECT")->UpdateShaderVariable(_commandList);
	for (auto& object : m_vecObjects) {
		object->Render(_commandList);
	}
	m_pPlayer->Render(_commandList);

	m_mapShaders.at("SKYBOX")->UpdateShaderVariable(_commandList);
	m_pSkybox->Render(_commandList);
}

void CScene::BuildObjects(const ComPtr<ID3D12Device>& _device,
	const ComPtr<ID3D12GraphicsCommandList>& _commandList,
	const ComPtr<ID3D12RootSignature>& _rootSignature)
{
	auto objectShader = make_shared<CObjectShader>(_device, _rootSignature);
	m_mapShaders.insert({ "OBJECT", objectShader });
	auto skyboxShader = make_shared<CSkyboxShader>(_device, _rootSignature);
	m_mapShaders.insert({ "SKYBOX", skyboxShader });

	auto cube = make_shared<CCubeMesh>(_device, _commandList);
	m_mapMeshes.insert({ "CUBE", cube });
	auto skyboxMesh = make_shared<CSkyboxMesh>(_device, _commandList);
	m_mapMeshes.insert({ "SKYBOX", skyboxMesh });

	auto checkboardTexture = make_shared<CTexture>(_device, _commandList,
		TEXT("../HelloDinner/Resources/Textures/Rock.dds"), RootParameter::Texture);
	m_mapTextures.insert({ "CHECKBOARD", checkboardTexture });
	auto brickTextire = make_shared<CTexture>(_device, _commandList,
		TEXT("../HelloDinner/Resources/Textures/Rock.dds"), RootParameter::Texture);
	m_mapTextures.insert({ "BRICK", brickTextire });
	auto skyboxTexture = make_shared<CTexture>(_device, _commandList,
		TEXT("../HelloDinner/Resources/Textures/SkyBoX_Cube.dds"), RootParameter::TextureCube);
	m_mapTextures.insert({ "SKYBOX", skyboxTexture });

	m_pPlayer = make_shared<CPlayer>();
	m_pPlayer->SetMesh(cube);
	m_pPlayer->SetTexture(checkboardTexture);
	m_pPlayer->SetPosition(XMFLOAT3{ 0.f, 0.f, 0.f });

	for (int x = -15; x <= 15; x += 5) {
		for (int y = -15; y <= 15; y += 5) {
			for (int z = -15; z <= 15; z += 5) {
				auto object = make_shared<CRotatingObject>();
				object->SetMesh(cube);
				object->SetTexture(brickTextire);
				object->SetPosition(XMFLOAT3{
					static_cast<FLOAT>(x),
					static_cast<FLOAT>(y),
					static_cast<FLOAT>(z) });
				m_vecObjects.push_back(object);
			}
		}
	}

	m_pCamera = make_shared<ThirdPersonCamera>();
	m_pCamera->SetLens(0.25 * XM_PI, static_cast<FLOAT>(1280) / static_cast<FLOAT>(720), 0.1f, 1000.f);
	m_pPlayer->SetCamera(m_pCamera);

	m_pSkybox = make_shared<CGameObject>();
	m_pSkybox->SetMesh(skyboxMesh);
	m_pSkybox->SetTexture(skyboxTexture);
}

void CScene::ReleaseUploadBuffer()
{
	for (auto& mesh : views::values(m_mapMeshes)) {
		mesh->ReleaseUploadBuffer();
	}
	for (auto& texture : views::values(m_mapTextures)) {
		texture->ReleaseUploadBuffer();
	}

}

void CScene::MouseEvent(UINT _message, LPARAM _lParam)
{
}

void CScene::KeyboardEvent(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam)
{

}

