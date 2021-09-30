#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <WICTextureLoader.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Mesh.h"
#include "SimpleShader.h"


class Assets
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static Assets& GetInstance()
	{
		if (!instance)
		{
			instance = new Assets();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	Assets(Assets const&) = delete;
	void operator=(Assets const&) = delete;

private:
	static Assets* instance;
	Assets() {};
#pragma endregion

public:
	~Assets();

	void Initialize(std::string rootAssetPath, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void LoadAllAssets();
	void LoadPixelShader(std::string path, bool useAssetPath = false);
	void LoadVertexShader(std::string path, bool useAssetPath = false);

	void CreateSolidColorTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4 color);

	Mesh* GetMesh(std::string name);
	SimplePixelShader* GetPixelShader(std::string name);
	SimpleVertexShader* GetVertexShader(std::string name);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(std::string name);

private:

	void LoadMesh(std::string path);
	void LoadTexture(std::string path);
	void LoadDDSTexture(std::string path);
	void LoadUnknownShader(std::string path);

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	std::string rootAssetPath;

	std::unordered_map<std::string, Mesh*> meshes;
	std::unordered_map<std::string, SimplePixelShader*> pixelShaders;
	std::unordered_map<std::string, SimpleVertexShader*> vertexShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;

	// Helpers for determining the actual path to the executable
	std::string GetExePath();
	std::wstring GetExePath_Wide();

	std::string GetFullPathTo(std::string relativeFilePath);
	std::wstring GetFullPathTo_Wide(std::wstring relativeFilePath);

	bool EndsWith(std::string str, std::string ending);
	std::wstring ToWideString(std::string str);
};

