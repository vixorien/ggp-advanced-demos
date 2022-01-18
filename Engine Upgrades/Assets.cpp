#include "Assets.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
// If using C++17, remove the "experimental" portion above and anywhere filesystem is used!

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>


// Singleton requirement
Assets* Assets::instance;


// --------------------------------------------------------------------------
// Cleans up the asset manager and deletes any resources that are
// not stored with smart pointers.
// --------------------------------------------------------------------------
Assets::~Assets()
{
}



// --------------------------------------------------------------------------
// Initializes the asset manager with the D3D objects it may need, as 
// well as the root asset path to check for assets.  Note that shaders
// are loaded from the executables path by default.
// --------------------------------------------------------------------------
void Assets::Initialize(std::string rootAssetPath, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, bool printLoadingProgress, bool allowOnDemandLoading)
{
	this->device = device;
	this->context = context;
	this->rootAssetPath = rootAssetPath;
	this->printLoadingProgress = printLoadingProgress;
	this->allowOnDemandLoading = allowOnDemandLoading;

	// Replace all "\\" with "/" to ease lookup later
	std::replace(this->rootAssetPath.begin(), this->rootAssetPath.end(), '\\', '/');

	// Ensure the root path ends with a slash
	if (!EndsWith(rootAssetPath, "/"))
		rootAssetPath += "/";
}


// --------------------------------------------------------------------------
// Recursively checks all files starting in the root asset path, determines
// if they are files that can be loaded and loads each one.
// 
// Currently, only the following file types are supported:
//  - Textures: .jpg, .png, .dds
//  - Meshes: .obj
//  - Sprite Font: .spritefont
//  - Shaders: .cso (these are loaded from the executable's path!)
// --------------------------------------------------------------------------
void Assets::LoadAllAssets()
{
	if (rootAssetPath.empty())
		return;

	// Recursively go through all directories starting at the root
	for (auto& item : std::experimental::filesystem::recursive_directory_iterator(GetFullPathTo(rootAssetPath)))
	{
		// Is this a regular file?
		if (item.status().type() == std::experimental::filesystem::file_type::regular)
		{
			std::string itemPath = item.path().string();

			// Replace all "\\" with "/" to ease lookup later
			std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

			// Determine the file type
			if (EndsWith(itemPath, ".obj"))
			{
				LoadMesh(itemPath);
			}
			else if (EndsWith(itemPath, ".jpg") || EndsWith(itemPath, ".png"))
			{
				LoadTexture(itemPath);
			}
			else if (EndsWith(itemPath, ".dds"))
			{
				LoadDDSTexture(itemPath);
			}
			else if (EndsWith(itemPath, ".spritefont"))
			{
				LoadSpriteFont(itemPath);
			}
		}
	}

	// Search and load all shaders in the exe directory
	for (auto& item : std::experimental::filesystem::directory_iterator(GetFullPathTo(".")))
	{
		// Assume we're just using the filename for shaders due to being in the .exe path
		std::string itemPath = item.path().filename().string();

		// Is this a Compiled Shader Object?
		if (EndsWith(itemPath, ".cso"))
		{
			LoadUnknownShader(itemPath);
		}
	}
}



// --------------------------------------------------------------------------
// Gets the specified mesh if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the mesh if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
//
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use "/" as the folder separator
//  - Do NOT include the file extension
//  - Example: "Models/cube"
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::GetMesh(std::string name)
{
	// Search and return mesh if found
	auto it = meshes.find(name);
	if (it != meshes.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::string filePath = GetFullPathTo(rootAssetPath + name + ".obj");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadMesh(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified sprite font if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the sprite font if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use "/" as the folder separator
//  - Do NOT include the file extension
//  - Example: "Fonts/Arial12"
// --------------------------------------------------------------------------
std::shared_ptr<DirectX::SpriteFont> Assets::GetSpriteFont(std::string name)
{
	// Search and return mesh if found
	auto it = spriteFonts.find(name);
	if (it != spriteFonts.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::string filePath = GetFullPathTo(rootAssetPath + name + ".spritefont");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadSpriteFont(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified texture if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the texture if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use "/" as the folder separator
//  - Do NOT include the file extension
//  - Example: "Textures/PBR/cobblestone_albedo"
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::GetTexture(std::string name)
{
	// Search and return texture if found
	auto it = textures.find(name);
	if (it != textures.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::string filePathJPG = GetFullPathTo(rootAssetPath + name + ".jpg");
		if (std::experimental::filesystem::exists(filePathJPG)) { return LoadTexture(filePathJPG); }

		// Is it a PNG?
		std::string filePathPNG = GetFullPathTo(rootAssetPath + name + ".png");
		if (std::experimental::filesystem::exists(filePathPNG)) { return LoadTexture(filePathPNG); }

		// Is it a DDS?
		std::string filePathDDS = GetFullPathTo(rootAssetPath + name + ".dds");
		if (std::experimental::filesystem::exists(filePathDDS)) { return LoadDDSTexture(filePathDDS); }
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified pixel shader if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the pixel shader if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do NOT include the path, unless it is outside the executable's folder
//  - Do use "/" as the folder separator
//  - Do NOT include the file extension
//  - Example: "SkyPS"
// --------------------------------------------------------------------------
std::shared_ptr<SimplePixelShader> Assets::GetPixelShader(std::string name)
{
	// Search and return shader if found
	auto it = pixelShaders.find(name);
	if (it != pixelShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::string filePath = name + ".cso";
		if (std::experimental::filesystem::exists(GetFullPathTo(filePath)))
		{
			// Attempt to load the pixel shader and return it if successful
			std::shared_ptr<SimplePixelShader> ps = LoadPixelShader(filePath);
			if (ps) { return ps; }
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified vertex shader if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the vertex shader if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do NOT include the path, unless it is outside the executable's folder
//  - Do use "/" as the folder separator
//  - Do NOT include the file extension
//  - Example: "SkyVS"
// --------------------------------------------------------------------------
std::shared_ptr<SimpleVertexShader> Assets::GetVertexShader(std::string name)
{
	// Search and return shader if found
	auto it = vertexShaders.find(name);
	if (it != vertexShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::string filePath = name + ".cso";
		if (std::experimental::filesystem::exists(GetFullPathTo(filePath)))
		{
			// Attempt to load the pixel shader and return it if successful
			std::shared_ptr<SimpleVertexShader> vs = LoadVertexShader(filePath);
			if (vs) { return vs; }
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Adds an existing mesh to the asset manager.
// 
// Note: The asset manager will clean up (delete) this mesh at shut down.
// Make sure you're not also deleting it yourself later.
// --------------------------------------------------------------------------
void Assets::AddMesh(std::string name, std::shared_ptr<Mesh> mesh)
{
	meshes.insert({ name, mesh });
}


// --------------------------------------------------------------------------
// Adds an existing sprite font to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddSpriteFont(std::string name, std::shared_ptr<DirectX::SpriteFont> font)
{
	spriteFonts.insert({ name, font });
}


// --------------------------------------------------------------------------
// Adds an existing pixel shader to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddPixelShader(std::string name, std::shared_ptr<SimplePixelShader> ps)
{
	pixelShaders.insert({ name, ps });
}


// --------------------------------------------------------------------------
// Adds an existing vertex shader to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddVertexShader(std::string name, std::shared_ptr<SimpleVertexShader> vs)
{
	vertexShaders.insert({ name, vs });
}


// --------------------------------------------------------------------------
// Adds an existing texture to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddTexture(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture)
{
	textures.insert({ name, texture });
}


// --------------------------------------------------------------------------
// Getters for asset counts
// --------------------------------------------------------------------------
unsigned int Assets::GetMeshCount() { return (unsigned int)meshes.size(); }
unsigned int Assets::GetSpriteFontCount() { return (unsigned int)spriteFonts.size(); }
unsigned int Assets::GetPixelShaderCount() { return (unsigned int)pixelShaders.size(); }
unsigned int Assets::GetVertexShaderCount() { return (unsigned int)vertexShaders.size(); }
unsigned int Assets::GetTextureCount() { return (unsigned int)textures.size(); }



// --------------------------------------------------------------------------
// Private helper for loading a mesh from an .obj file
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::LoadMesh(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading mesh: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<Mesh> m = std::make_shared<Mesh>(path.c_str(), device);

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	meshes.insert({ filename, m });
	return m;
}


// --------------------------------------------------------------------------
// Private helper for loading a .spritefont file
// --------------------------------------------------------------------------
std::shared_ptr<DirectX::SpriteFont> Assets::LoadSpriteFont(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading sprite font: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<DirectX::SpriteFont> font = std::make_shared<DirectX::SpriteFont>(device.Get(), ToWideString(path).c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	spriteFonts.insert({ filename, font });
	return font;
}



// --------------------------------------------------------------------------
// Private helper for loading a standard BMP/PNG/JPG/TIF texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::LoadTexture(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateWICTextureFromFile(device.Get(), context.Get(), ToWideString(path).c_str(), 0, srv.GetAddressOf());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}



// --------------------------------------------------------------------------
// Private helper for loading a DDS texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::LoadDDSTexture(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateDDSTextureFromFile(device.Get(), context.Get(), ToWideString(path).c_str(), 0, srv.GetAddressOf());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}


// --------------------------------------------------------------------------
// Private helper for loading a compiled shader object (.cso) of unknown type.
// Currently, this supports vertex and pixel shaders
// --------------------------------------------------------------------------
void Assets::LoadUnknownShader(std::string path)
{
	// Load the file into a blob
	ID3DBlob* shaderBlob;
	HRESULT hr = D3DReadFileToBlob(GetFullPathTo_Wide(ToWideString(path)).c_str(), &shaderBlob);
	if (hr != S_OK)
	{
		return;
	}

	// Set up shader reflection to get information about
	// this shader and its variables,  buffers, etc.
	ID3D11ShaderReflection* refl;
	D3DReflect(
		shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)&refl);

	// Get the description of the shader
	D3D11_SHADER_DESC shaderDesc;
	refl->GetDesc(&shaderDesc);

	// What kind of shader?
	switch (D3D11_SHVER_GET_TYPE(shaderDesc.Version))
	{
	case D3D11_SHVER_VERTEX_SHADER: LoadVertexShader(path); break;
	case D3D11_SHVER_PIXEL_SHADER: LoadPixelShader(path); break;
	}

	// Clean up
	refl->Release();
	shaderBlob->Release();
}



// --------------------------------------------------------------------------
// Private helper for loading a pixel shader from a .cso file
// --------------------------------------------------------------------------
std::shared_ptr<SimplePixelShader> Assets::LoadPixelShader(std::string path, bool useAssetPath)
{
	// Assuming filename and path are the same
	std::string filename = path;

	// Unless we need to check asset folder
	if (useAssetPath)
	{
		// Strip out everything before and including the asset root path
		size_t assetPathLength = rootAssetPath.size();
		size_t assetPathPosition = path.rfind(rootAssetPath);
		filename = path.substr(assetPathPosition + assetPathLength);
	}

	if (printLoadingProgress)
	{
		printf("Loading pixel shader: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	std::shared_ptr<SimplePixelShader> ps = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(ToWideString(path)).c_str());
	if (!ps->IsShaderValid()) { return 0; }

	// Success
	pixelShaders.insert({ filename, ps });
	return ps;

}


// --------------------------------------------------------------------------
// Private helper for loading a vertex shader from a .cso file
// --------------------------------------------------------------------------
std::shared_ptr<SimpleVertexShader> Assets::LoadVertexShader(std::string path, bool useAssetPath)
{
	// Assuming filename and path are the same
	std::string filename = path;

	// Unless we need to check asset folder
	if (useAssetPath)
	{
		// Strip out everything before and including the asset root path
		size_t assetPathLength = rootAssetPath.size();
		size_t assetPathPosition = path.rfind(rootAssetPath);
		filename = path.substr(assetPathPosition + assetPathLength);
	}

	if (printLoadingProgress)
	{
		printf("Loading vertex shader: ");
		printf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	std::shared_ptr<SimpleVertexShader> vs = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(ToWideString(path)).c_str());
	if (!vs->IsShaderValid()) { return 0; }

	// Success
	vertexShaders.insert({ filename, vs });
	return vs;
}



// --------------------------------------------------------------------------
// Creates a solid color texture of the specified size and adds it to
// the asset manager using the specified name.  
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// color - color for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateSolidColorTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4 color)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Create an array of the color
	DirectX::XMFLOAT4* pixels = new DirectX::XMFLOAT4[width * height];
	for (int i = 0; i < width * height; i++)
	{
		pixels[i] = color;
	}

	// Create the texture itself
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = CreateTexture(textureName, width, height, pixels);

	// All done with pixel array
	delete[] pixels;

	// Return the SRV in the event it is immediately needed
	return srv;
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Convert to ints
	unsigned char* intPixels = new unsigned char[width * height * 4];
	for (int i = 0; i < width * height * 4;)
	{
		int pixelIndex = i / 4;
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].x * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].y * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].z * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].w * 255);
	}

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = intPixels;
	data.SysMemPitch = sizeof(unsigned char) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = td.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

	// Add to the asset manager
	textures.insert({ textureName, srv });

	// All done with these values
	delete[] intPixels;

	// Return the SRV in the event it is immediately needed
	return srv;
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name.
// The resulting texture will hold arbitrary float values instead of 0-1 values.
// The texture format will be DXGI_FORMAT_R32G32B32A32_FLOAT.
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateFloatTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = sizeof(float) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = td.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

	// Add to the asset manager
	textures.insert({ textureName, srv });

	// Return the SRV in the event it is immediately needed
	return srv;
}


// --------------------------------------------------------------------------
// Gets the actual path to this executable
//
// - As it turns out, the relative path for a program is different when 
//    running through VS and when running the .exe directly, which makes 
//    it a pain to properly load files external files (like textures)
//    - Running through VS: Current Dir is the *project folder*
//    - Running from .exe:  Current Dir is the .exe's folder
// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
//    Visual Studio "thing", and isn't obvious unless you know to look 
//    for it.  In fact, it could be fixed by changing a setting in VS, but
//    the option is stored in a user file (.suo), which is ignored by most
//    version control packages by default.  Meaning: the option must be
//    changed every on every PC.  Ugh.  So instead, here's a helper.
// --------------------------------------------------------------------------
std::string Assets::GetExePath()
{
	// Assume the path is just the "current directory" for now
	std::string path = ".\\";

	// Get the real, full path to this executable
	char currentDir[1024] = {};
	GetModuleFileName(0, currentDir, 1024);

	// Find the location of the last slash charaacter
	char* lastSlash = strrchr(currentDir, '\\');
	if (lastSlash)
	{
		// End the string at the last slash character, essentially
		// chopping off the exe's file name.  Remember, c-strings
		// are null-terminated, so putting a "zero" character in 
		// there simply denotes the end of the string.
		*lastSlash = 0;

		// Set the remainder as the path
		path = currentDir;
	}

	// Toss back whatever we've found
	return path;
}


// ---------------------------------------------------
//  Same as GetExePath(), except it returns a wide character
//  string, which most of the Windows API requires.
// ---------------------------------------------------
std::wstring Assets::GetExePath_Wide()
{
	// Grab the path as a standard string
	std::string path = GetExePath();

	// Convert to a wide string
	wchar_t widePath[1024] = {};
	mbstowcs_s(0, widePath, path.c_str(), 1024);

	// Create a wstring for it and return
	return std::wstring(widePath);
}


// ----------------------------------------------------
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::string Assets::GetFullPathTo(std::string relativeFilePath)
{
	return GetExePath() + "\\" + relativeFilePath;
}



// ----------------------------------------------------
//  Same as GetFullPathTo, but with wide char strings.
// 
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::wstring Assets::GetFullPathTo_Wide(std::wstring relativeFilePath)
{
	return GetExePath_Wide() + L"\\" + relativeFilePath;
}


// ----------------------------------------------------
// Determines if the given string ends with the given ending
// ----------------------------------------------------
bool Assets::EndsWith(std::string str, std::string ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}



// ----------------------------------------------------
// Converts a standard string to a wide character string
// ----------------------------------------------------
std::wstring Assets::ToWideString(std::string str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(str);
}


// ----------------------------------------------------
// Remove the file extension by searching for the last 
// period character and removing everything afterwards
// ----------------------------------------------------
std::string Assets::RemoveFileExtension(std::string str)
{
	size_t found = str.find_last_of('.');
	return str.substr(0, found);
}
