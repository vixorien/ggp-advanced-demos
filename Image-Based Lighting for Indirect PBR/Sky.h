#pragma once

#include "Mesh.h"
#include "SimpleShader.h"
#include "Camera.h"

#include <wrl/client.h> // Used for ComPtr

class Sky
{
public:

	// Constructor that loads a DDS cube map file
	Sky(
		const wchar_t* cubemapDDSFile,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions, 	
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);

	// Constructor that takes an existing cube map SRV
	Sky(
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeMap,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);

	// Constructor that loads 6 textures and makes a cube map
	Sky(
		const wchar_t* right,
		const wchar_t* left,
		const wchar_t* up,
		const wchar_t* down,
		const wchar_t* front,
		const wchar_t* back,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);

	// Constructor that takes 6 existing SRVs and makes a cube map
	Sky(
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> right,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> left,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> up,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> down,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> front,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> back,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);

	~Sky();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetEnvironmentMap();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetIrradianceMap();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSpecularMap();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetBRDFLookUpTexture();
	int GetTotalSpecularIBLMipLevels();

	void Draw(Camera* camera);

private:

	void InitRenderStates();

	// Helper for creating a cubemap from 6 individual textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateCubemap(
		const wchar_t* right,
		const wchar_t* left,
		const wchar_t* up,
		const wchar_t* down,
		const wchar_t* front,
		const wchar_t* back);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateCubemap(
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> right,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> left,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> up,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> down,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> front,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> back);

	// IBL precompute steps
	void IBLCreateIrradianceMap();
	void IBLCreateConvolvedSpecularMap();
	void IBLCreateBRDFLookUpTexture();

	const int IBLCubeSize = 256;
	const int IBLLookUpTextureSize = 256;
	const int specIBLMipLevelsToSkip = 3; // Number of lower mips (1x1, 2x2, etc.) to exclude from the maps
	int totalSpecIBLMipLevels;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skySRV;			// Skybox
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> irradianceIBL;		// Incoming diffuse light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularIBL;		// Incoming specular light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brdfLookUpMap;		// Holds some pre-calculated BRDF results

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> skyRasterState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> skyDepthState;

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11Device> device;

};

