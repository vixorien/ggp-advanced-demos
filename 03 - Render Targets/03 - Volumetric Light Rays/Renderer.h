#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>

#include "Lights.h"
#include "Scene.h"

enum RenderTargetType
{
	SCENE_COLORS,
	SUN_AND_OCCLUDERS,

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
	int IndirectLightingEnabled;
	float IBLIntensity;
};

class Renderer
{
public:

	Renderer(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);


	void PreResize();
	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);

	void FrameStart();
	void FrameEnd(bool vsync, std::shared_ptr<Camera> camera);
	void RenderSimple(std::shared_ptr<Scene> scene, unsigned int activeLightCount);
	void RenderOptimized(std::shared_ptr<Scene> scene, unsigned int activeLightCount);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetRenderTargetSRV(RenderTargetType type);

	// Public-facing options (for UI interaction)
	bool indirectLighting;
	float iblIntensity;

	// Light ray parameters
	int lightRaySamples;
	float lightRayDensity;
	float lightRaySampleWeight;
	float lightRayDecay;
	float lightRayExposure;
	DirectX::XMFLOAT3 lightRaySunDirection;
	float lightRaySunFalloffExponent;
	DirectX::XMFLOAT3 lightRaySunColor;
	bool lightRayUseSkyboxColor;

private:

	// The renderer needs access to all the core D3D stuff
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

	// D3D rendering resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;

	// Render targets
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetRTVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> renderTargetSRVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	
	void CreateRenderTarget(
		unsigned int width,
		unsigned int height,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
		DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

	void PostProcess(std::shared_ptr<Camera> camera);

	// Window-related
	unsigned int windowWidth;
	unsigned int windowHeight;

	// Per-frame constant buffers and data
	Microsoft::WRL::ComPtr<ID3D11Buffer> psPerFrameConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vsPerFrameConstantBuffer;
	PSPerFrameData psPerFrameData;
	VSPerFrameData vsPerFrameData;
};

