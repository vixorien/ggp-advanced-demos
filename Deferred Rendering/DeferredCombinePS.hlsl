
#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	matrix InvViewProj;
	float3 CameraPosition;
	int SpecIBLTotalMipLevels;
	float IBLIntensity;
}

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

struct SceneOutput
{
	float4 SceneNoAmbient	: SV_TARGET0;
	float4 SceneAmbient		: SV_TARGET1;
};

// Textures and such
Texture2D GBufferAlbedo			: register(t0);
Texture2D GBufferNormals		: register(t1);
Texture2D GBufferDepth			: register(t2);
Texture2D GBufferMetalRough		: register(t3);

Texture2D LightBuffer			: register(t4);

// IBL (indirect PBR) textures
Texture2D BrdfLookUpMap			: register(t5);
TextureCube IrradianceIBLMap	: register(t6);
TextureCube SpecularIBLMap		: register(t7);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);


// Entry point for this pixel shader
SceneOutput main(VertexToPixel input)
{
	float3 pixelIndex = float3(input.position.xy, 0);
	
	float3 surfaceColor = GBufferAlbedo.Load(pixelIndex).rgb;
	float3 normal		= normalize(GBufferNormals.Load(pixelIndex).rgb * 2 - 1);
	float  depth		= GBufferDepth.Load(pixelIndex).r;
	float3 metalRough	= GBufferMetalRough.Load(pixelIndex).rgb;
	
	float metal = metalRough.r;
	float roughness = metalRough.g;

	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Calc world position from depth
	float3 worldPos = WorldSpaceFromDepth(depth, input.uv, InvViewProj);

	// Calculate requisite reflection vectors
	float3 viewToCam = normalize(CameraPosition - worldPos);
	float3 viewRefl = normalize(reflect(-viewToCam, normal));
	float NdotV = saturate(dot(normal, viewToCam));

	// Indirect lighting
	float3 indirectDiffuse = IndirectDiffuse(IrradianceIBLMap, BasicSampler, normal) * IBLIntensity;
	float3 indirectSpecular = IndirectSpecular(
		SpecularIBLMap, SpecIBLTotalMipLevels,
		BrdfLookUpMap, ClampSampler, // MUST use the clamp sampler here!
		viewRefl, NdotV,
		roughness, specColor) * IBLIntensity;

	// Balance indirect diff/spec
	float3 balancedIndirectDiff = DiffuseEnergyConserve(indirectDiffuse, specColor, metal) * surfaceColor.rgb;

	// Set up output
	SceneOutput output;
	output.SceneNoAmbient = float4(LightBuffer.Load(pixelIndex).rgb + indirectSpecular, 1);
	output.SceneAmbient = float4(balancedIndirectDiff, 1);
	return output;
}