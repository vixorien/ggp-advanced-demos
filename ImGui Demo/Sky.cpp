#include "Sky.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"

using namespace DirectX;

Sky::Sky(
	const wchar_t* cubemapDDSFile, 
	Mesh* mesh, 
	SimpleVertexShader* skyVS, 
	SimplePixelShader* skyPS, 
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions, 
	Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Save params
	this->skyMesh = mesh;
	this->device = device;
	this->context = context;
	this->samplerOptions = samplerOptions;
	this->skyVS = skyVS;
	this->skyPS = skyPS;

	// Init render states
	InitRenderStates();

	// Load texture
	CreateDDSTextureFromFile(device.Get(), cubemapDDSFile, 0, skySRV.GetAddressOf());
}

Sky::Sky(
	const wchar_t* right, 
	const wchar_t* left, 
	const wchar_t* up, 
	const wchar_t* down, 
	const wchar_t* front, 
	const wchar_t* back, 
	Mesh* mesh,
	SimpleVertexShader* skyVS,
	SimplePixelShader* skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Save params
	this->skyMesh = mesh;
	this->device = device;
	this->context = context;
	this->samplerOptions = samplerOptions;
	this->skyVS = skyVS;
	this->skyPS = skyPS;

	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skySRV = CreateCubemap(right, left, up, down, front, back);
}

Sky::~Sky()
{
}

void Sky::Draw(Camera* camera)
{
	// Change to the sky-specific rasterizer state
	context->RSSetState(skyRasterState.Get());
	context->OMSetDepthStencilState(skyDepthState.Get(), 0);

	// Set the sky shaders
	skyVS->SetShader();
	skyPS->SetShader();

	// Give them proper data
	skyVS->SetMatrix4x4("view", camera->GetView());
	skyVS->SetMatrix4x4("projection", camera->GetProjection());
	skyVS->CopyAllBufferData();

	// Send the proper resources to the pixel shader
	skyPS->SetShaderResourceView("skyTexture", skySRV);
	skyPS->SetSamplerState("samplerOptions", samplerOptions);

	// Set mesh buffers and draw
	skyMesh->SetBuffersAndDraw(context);

	// Reset my rasterizer state to the default
	context->RSSetState(0); // Null (or 0) puts back the defaults
	context->OMSetDepthStencilState(0, 0);
}

void Sky::InitRenderStates()
{
	// Rasterizer to reverse the cull mode
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.CullMode = D3D11_CULL_FRONT; // Draw the inside instead of the outside!
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&rastDesc, skyRasterState.GetAddressOf());

	// Depth state so that we ACCEPT pixels with a depth == 1
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	device->CreateDepthStencilState(&depthDesc, skyDepthState.GetAddressOf());
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::CreateCubemap(const wchar_t* right, const wchar_t* left, const wchar_t* up, const wchar_t* down, const wchar_t* front, const wchar_t* back)
{
	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not the SHADER RESOURCE VIEWS!
	// - Specifically NOT generating mipmaps, as we don't need them for the sky!
	// - Order matters here!  +X, -X, +Y, -Y, +Z, -Z
	ID3D11Texture2D* textures[6] = {};
	CreateWICTextureFromFile(device.Get(), right, (ID3D11Resource**)&textures[0], 0);
	CreateWICTextureFromFile(device.Get(), left, (ID3D11Resource**)&textures[1], 0);
	CreateWICTextureFromFile(device.Get(), up, (ID3D11Resource**)&textures[2], 0);
	CreateWICTextureFromFile(device.Get(), down, (ID3D11Resource**)&textures[3], 0);
	CreateWICTextureFromFile(device.Get(), front, (ID3D11Resource**)&textures[4], 0);
	CreateWICTextureFromFile(device.Get(), back, (ID3D11Resource**)&textures[5], 0);

	// We'll assume all of the textures are the same color format and resolution,
	// so get the description of the first shader resource view
	D3D11_TEXTURE2D_DESC faceDesc = {};
	textures[0]->GetDesc(&faceDesc);

	// Describe the resource for the cube map, which is simply 
	// a "texture 2d array".  This is a special GPU resource format, 
	// NOT just a C++ array of textures!!!
	D3D11_TEXTURE2D_DESC cubeDesc = {};
	cubeDesc.ArraySize = 6; // Cube map!
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We'll be using as a texture in a shader
	cubeDesc.CPUAccessFlags = 0; // No read back
	cubeDesc.Format = faceDesc.Format; // Match the loaded texture's color format
	cubeDesc.Width = faceDesc.Width;  // Match the size
	cubeDesc.Height = faceDesc.Height; // Match the size
	cubeDesc.MipLevels = 1; // Only need 1
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // This should be treated as a CUBE, not 6 separate textures
	cubeDesc.Usage = D3D11_USAGE_DEFAULT; // Standard usage
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the actual texture resource
	ID3D11Texture2D* cubeMapTexture = 0;
	device->CreateTexture2D(&cubeDesc, 0, &cubeMapTexture);

	// Loop through the individual face textures and copy them,
	// one at a time, to the cube map texure
	for (int i = 0; i < 6; i++)
	{
		// Calculate the subresource position to copy into
		unsigned int subresource = D3D11CalcSubresource(
			0,	// Which mip (zero, since there's only one)
			i,	// Which array element?
			1); // How many mip levels are in the texture?

		// Copy from one resource (texture) to another
		context->CopySubresourceRegion(
			cubeMapTexture, // Destination resource
			subresource,	// Dest subresource index (one of the array elements)
			0, 0, 0,		// XYZ location of copy
			textures[i],	// Source resource
			0,				// Source subresource index (we're assuming there's only one)
			0);				// Source subresource "box" of data to copy (zero means the whole thing)
	}

	// At this point, all of the faces have been copied into the 
	// cube map texture, so we can describe a shader resource view for it
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format; // Same format as texture
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	srvDesc.TextureCube.MipLevels = 1;	// Only need access to 1 mip
	srvDesc.TextureCube.MostDetailedMip = 0; // Index of the first mip we want to see

	// Make the SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeSRV;
	device->CreateShaderResourceView(cubeMapTexture, &srvDesc, cubeSRV.GetAddressOf());

	// Now that we're done, clean up the stuff we don't need anymore
	cubeMapTexture->Release();  // Done with this particular reference (the SRV has another)
	for (int i = 0; i < 6; i++)
		textures[i]->Release();

	// Send back the SRV, which is what we need for our shaders
	return cubeSRV;
}
