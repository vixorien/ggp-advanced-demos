#pragma once

#include <d3d11.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <WICTextureLoader.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteFont.h>

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
	Assets() : 
		allowOnDemandLoading(true),
		printLoadingProgress(false)	{};
#pragma endregion

public:
	~Assets();

	void Initialize(
		std::wstring rootAssetPath,
		std::wstring rootShaderPath,
		Microsoft::WRL::ComPtr<ID3D11Device> device, 
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
		bool printLoadingProgress = false,
		bool allowOnDemandLoading = true);

	void LoadAllAssets();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColorTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4 color);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4* pixels);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateFloatTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4* pixels);

	std::shared_ptr<Mesh> GetMesh(std::wstring name);
	std::shared_ptr<DirectX::SpriteFont> GetSpriteFont(std::wstring name);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(std::wstring name);
	std::shared_ptr<SimplePixelShader> GetPixelShader(std::wstring name);
	std::shared_ptr<SimpleVertexShader> GetVertexShader(std::wstring name);

	void AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh);
	void AddSpriteFont(std::wstring name, std::shared_ptr<DirectX::SpriteFont> font);
	void AddPixelShader(std::wstring name, std::shared_ptr<SimplePixelShader> ps);
	void AddVertexShader(std::wstring name, std::shared_ptr<SimpleVertexShader> vs);
	void AddTexture(std::wstring name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture);

	unsigned int GetMeshCount();
	unsigned int GetSpriteFontCount();
	unsigned int GetPixelShaderCount();
	unsigned int GetVertexShaderCount();
	unsigned int GetTextureCount();

private:

	std::shared_ptr<Mesh> LoadMesh(std::wstring path);
	std::shared_ptr<DirectX::SpriteFont> LoadSpriteFont(std::wstring path);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTexture(std::wstring path);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadDDSTexture(std::wstring path);
	void LoadUnknownShader(std::wstring path);
	std::shared_ptr<SimplePixelShader> LoadPixelShader(std::wstring path);
	std::shared_ptr<SimpleVertexShader> LoadVertexShader(std::wstring path);

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	std::wstring rootAssetPath;
	std::wstring rootShaderPath;
	bool printLoadingProgress;

	bool allowOnDemandLoading;

	std::unordered_map<std::wstring, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<std::wstring, std::shared_ptr<DirectX::SpriteFont>> spriteFonts;
	std::unordered_map<std::wstring, std::shared_ptr<SimplePixelShader>> pixelShaders;
	std::unordered_map<std::wstring, std::shared_ptr<SimpleVertexShader>> vertexShaders;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;

	// Helpers for paths
	bool EndsWith(std::wstring str, std::wstring ending);
	std::wstring RemoveFileExtension(std::wstring str);
};

