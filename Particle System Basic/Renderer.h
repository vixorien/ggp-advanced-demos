#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Camera.h"
#include "GameEntity.h"
#include "Lights.h"
#include "Sky.h"
#include "Emitter.h"

enum RenderTargetType
{
	SCENE_COLORS_NO_AMBIENT,
	SCENE_AMBIENT,
	SCENE_NORMALS,
	SCENE_DEPTHS,
	SSAO_RESULTS,
	SSAO_BLUR,

	// Count is always the last one!
	RENDER_TARGET_TYPE_COUNT
};

// This needs to match the expected per-frame vertex shader data
struct VSPerFrameData
{
	DirectX::XMFLOAT4X4 ViewMatrix;
	DirectX::XMFLOAT4X4 ProjectionMatrix;
};

// This needs to match the expected per-frame pixel shader data
struct PSPerFrameData
{
	Light Lights[MAX_LIGHTS];
	int LightCount;
	DirectX::XMFLOAT3 CameraPosition;
	int TotalSpecIBLMipLevels;
	DirectX::XMFLOAT3 AmbientNonPBR;
};

class Renderer
{

public:
	Renderer(
		const std::vector<GameEntity*>& entities,
		const std::vector<Light>& lights,
		const std::vector<Emitter*>& emitters,
		unsigned int activeLightCount,
		Sky* sky,
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11Device> device, 
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain, 
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);
	~Renderer();

	void Render(Camera* camera, float totalTime);
	void PreResize();
	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);

	unsigned int GetActiveLightCount();
	void SetActiveLightCount(unsigned int count);

	void SetPointLightsVisible(bool visible);
	bool GetPointLightsVisible();

	void SetSSAOEnabled(bool enabled);
	bool GetSSAOEnabled();

	void SetSSAORadius(float radius);
	float GetSSAORadius();

	void SetSSAOSamples(int samples);
	int GetSSAOSamples();

	void SetSSAOOutputOnly(bool ssaoOnly);
	bool GetSSAOOutputOnly();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetRenderTargetSRV(RenderTargetType type);

private:
	// The renderer needs access to all the core DX stuff
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

	// DX resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;

	// Render targets
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetRTVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> renderTargetSRVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];

	// Particle states
	Microsoft::WRL::ComPtr<ID3D11BlendState> particleBlendAdditive; 
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> particleDepthState;

	// SSAO variables
	DirectX::XMFLOAT4 ssaoOffsets[64];
	int ssaoSamples;
	float ssaoRadius;
	bool ssaoEnabled;
	bool ssaoOutputOnly;

	// Overall ambient for non-pbr shaders
	DirectX::XMFLOAT3 ambientNonPBR;

	// Window-related
	unsigned int windowWidth;
	unsigned int windowHeight;

	// References to lists from Game
	const std::vector<GameEntity*>& entities;
	const std::vector<Light>& lights;
	const std::vector<Emitter*>& emitters;
	Sky* sky;
	unsigned int activeLightCount;

	// Per-frame constant buffers and data
	Microsoft::WRL::ComPtr<ID3D11Buffer> psPerFrameConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vsPerFrameConstantBuffer;
	PSPerFrameData psPerFrameData;
	VSPerFrameData vsPerFrameData;

	// Note: Potentially replace this with an instanced "debug drawing" set of methods?
	void DrawPointLights(Camera* camera);
	bool pointLightsVisible;

	void CreateRenderTarget(
		unsigned int width, 
		unsigned int height, 
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
		DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
};

