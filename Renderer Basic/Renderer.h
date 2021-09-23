#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Camera.h"
#include "GameEntity.h"
#include "Lights.h"
#include "Sky.h"

class Renderer
{

public:
	Renderer(
		const std::vector<GameEntity*>& entities,
		const std::vector<Light>& lights,
		Sky* sky,
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11Device> device, 
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain, 
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);
	~Renderer();

	void Render(Camera* camera, int lightCount_RemoveMeLater);
	void PreResize();
	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);

private:
	// The renderer needs access to all the core DX stuff
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

	// DX resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;

	// Window-related
	unsigned int windowWidth;
	unsigned int windowHeight;

	// References to lists from Game
	const std::vector<GameEntity*>& entities;
	const std::vector<Light>& lights;
	Sky* sky;

	// Note: Potentially replace this with an instanced "debug drawing" set of methods?
	void DrawPointLights(Camera* camera, int lightCount_RemoveMeLater);
};

