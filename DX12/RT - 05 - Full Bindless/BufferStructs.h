#pragma once

#include "Lights.h"
#include <DirectXMath.h>

// Must match vertex shader definition!
struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldInverseTranspose;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

// Must match pixel shader definition!
struct PixelShaderExternalData
{
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
};

// Overall scene data for raytracing
struct RaytracingSceneData
{
	DirectX::XMFLOAT4X4 inverseViewProjection;
	DirectX::XMFLOAT3 cameraPosition;
	int raysPerPixel;
	int maxRecursionDepth;
	DirectX::XMFLOAT3 skyUpColor;
	DirectX::XMFLOAT3 skyDownColor;
	unsigned int accumulationFrameCount;
};

struct RaytracingMaterial
{
	DirectX::XMFLOAT3 color;
	float roughness;

	float metal;
	float emissiveIntensity;
	DirectX::XMFLOAT2 uvScale;

	unsigned int albedoIndex;
	unsigned int normalMapIndex;
	unsigned int roughnessIndex;
	unsigned int metalnessIndex;
};

// Ensure this matches Raytracing shader define!
#define MAX_INSTANCES_PER_BLAS 100
struct RaytracingEntityData
{
	RaytracingMaterial materials[MAX_INSTANCES_PER_BLAS];
};