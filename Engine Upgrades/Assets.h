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
		printLoadingProgress(false) {};
#pragma endregion

public:
	~Assets();

	void Initialize(
		std::string rootAssetPath,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		bool printLoadingProgress = false,
		bool allowOnDemandLoading = true);

	void LoadAllAssets();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColorTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4 color);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateFloatTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels);

	std::shared_ptr<Mesh> GetMesh(std::string name);
	std::shared_ptr<DirectX::SpriteFont> GetSpriteFont(std::string name);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(std::string name);
	std::shared_ptr<SimplePixelShader> GetPixelShader(std::string name);
	std::shared_ptr<SimpleVertexShader> GetVertexShader(std::string name);

	void AddMesh(std::string name, std::shared_ptr<Mesh> mesh);
	void AddSpriteFont(std::string name, std::shared_ptr<DirectX::SpriteFont> font);
	void AddPixelShader(std::string name, std::shared_ptr<SimplePixelShader> ps);
	void AddVertexShader(std::string name, std::shared_ptr<SimpleVertexShader> vs);
	void AddTexture(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture);

	unsigned int GetMeshCount();
	unsigned int GetSpriteFontCount();
	unsigned int GetPixelShaderCount();
	unsigned int GetVertexShaderCount();
	unsigned int GetTextureCount();

private:

	std::shared_ptr<Mesh> LoadMesh(std::string path);
	std::shared_ptr<DirectX::SpriteFont> LoadSpriteFont(std::string path);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTexture(std::string path);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadDDSTexture(std::string path);
	void LoadUnknownShader(std::string path);
	std::shared_ptr<SimplePixelShader> LoadPixelShader(std::string path, bool useAssetPath = false);
	std::shared_ptr<SimpleVertexShader> LoadVertexShader(std::string path, bool useAssetPath = false);

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	std::string rootAssetPath;
	bool printLoadingProgress;

	bool allowOnDemandLoading;

	std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<std::string, std::shared_ptr<DirectX::SpriteFont>> spriteFonts;
	std::unordered_map<std::string, std::shared_ptr<SimplePixelShader>> pixelShaders;
	std::unordered_map<std::string, std::shared_ptr<SimpleVertexShader>> vertexShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;

	// Helpers for determining the actual path to the executable
	std::string GetExePath();
	std::wstring GetExePath_Wide();

	std::string GetFullPathTo(std::string relativeFilePath);
	std::wstring GetFullPathTo_Wide(std::wstring relativeFilePath);

	bool EndsWith(std::string str, std::string ending);
	std::wstring ToWideString(std::string str);
	std::string RemoveFileExtension(std::string str);
};

