#pragma once

#include <memory>

#include "Mesh.h"
#include "SimpleShader.h"
#include "Camera.h"
#include "..\..\Common\json\json.hpp"

#include <wrl/client.h> // Used for ComPtr

class Sky
{
public:

	// Constructor that loads a DDS cube map file
	Sky(
		const wchar_t* cubemapDDSFile, 
		std::shared_ptr<Mesh> mesh,
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
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
		std::shared_ptr<Mesh> mesh,
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
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
		std::shared_ptr<Mesh> mesh,
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
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

	void Draw(std::shared_ptr<Camera> camera);

	static std::shared_ptr<Sky> Parse(
		nlohmann::json jsonSky,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

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

	// Skybox related resources
	std::shared_ptr<SimpleVertexShader> skyVS;
	std::shared_ptr<SimplePixelShader> skyPS;
	
	std::shared_ptr<Mesh> skyMesh;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> skyRasterState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> skyDepthState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skySRV;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11Device> device;

	// IBL precompute steps
	void IBLCreateIrradianceMap();
	void IBLCreateConvolvedSpecularMap();
	void IBLCreateBRDFLookUpTexture();

	const int IBLCubeSize = 256;
	const int IBLLookUpTextureSize = 256;
	const int specIBLMipLevelsToSkip = 3; // Number of lower mips (1x1, 2x2, etc.) to exclude from the maps
	int totalSpecIBLMipLevels;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> irradianceIBL;		// Incoming diffuse light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularIBL;		// Incoming specular light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brdfLookUpMap;		// Holds some pre-calculated BRDF results
};

