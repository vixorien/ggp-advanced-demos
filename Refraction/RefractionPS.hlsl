
#include "Lighting.hlsli"

// How many lights could we handle?
#define MAX_LIGHTS 128

// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// An array of light data
	Light Lights[MAX_LIGHTS];

	// The amount of lights THIS FRAME
	int LightCount;

	// Needed for specular (reflection) calculation
	float3 CameraPosition;

	// The number of mip levels in the specular IBL map
	int SpecIBLTotalMipLevels;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float4 Color;
};

// Data that usually changes per object
cbuffer perObject : register(b2)
{
	// These realistically change once per frame, but we're
	// setting here to make the renderer implementation easier
	matrix viewMatrix;
	float2 screenSize;
};


// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION; // The world position of this PIXEL
};


// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
Texture2D MetalTexture			: register(t3);

// IBL (indirect PBR) textures
Texture2D BrdfLookUpMap			: register(t4);
TextureCube IrradianceIBLMap	: register(t5);
TextureCube SpecularIBLMap		: register(t6);

// Refraction requirement
Texture2D ScreenPixels			: register(t7);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

// Fresnel term - Schlick approx.
float SimpleFresnel(float3 v, float3 n, float f0)
{
	// Pre-calculations
	float NdotV = saturate(dot(n, v));

	// Final value
	return f0 + (1 - f0) * pow(1 - NdotV, 5);
}

// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Sample various textures
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);
	float roughness = RoughnessTexture.Sample(BasicSampler, input.uv).r;
	float metal = MetalTexture.Sample(BasicSampler, input.uv).r;

	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * Color.rgb;

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Vars for controlling refraction - Adjust as you see fit
	float indexOfRefr = 0.4f; // Ideally keep this below 1 - not physically accurate, but prevents "total internal reflection"
	float refrAdjust = 0.1f;  // Makes our refraction less extreme, since we're using UV coords not world units

	// Calculate the refraction amount in WORLD SPACE
	float3 dirToPixel = normalize(input.worldPos - CameraPosition);
	float3 refrDir = refract(dirToPixel, input.normal, indexOfRefr);

	// Get the refraction XY direction in VIEW SPACE (relative to the camera)
	// We use this as a UV offset when sampling the texture
	float2 refrUV = mul(viewMatrix, float4(refrDir, 0.0f)).xy * refrAdjust;
	refrUV.x *= -1.0f; // Flip the X to point away from the edge (Y already does this due to view space <-> texture space diff)

	// Grab the scene color in the correct (offset) location
	float2 screenUV = input.screenPosition.xy / screenSize;
	float3 sceneColor = ScreenPixels.Sample(ClampSampler, screenUV + refrUV).rgb;


	// Instead, try screen space adjustment based on normal alone
	/*float3 normalViewSpace = mul(viewMatrix, float4(input.normal, 0.0f)).xyz;
	normalViewSpace.y *= -1;
	sceneColor = ScreenPixels.Sample(ClampSampler, screenUV + normalViewSpace.xy * refrAdjust).rgb;*/
	

	// Calculate requisite reflection vectors
	float3 viewToCam = normalize(CameraPosition - input.worldPos);
	float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	float NdotV = saturate(dot(input.normal, viewToCam));

	// Get reflections
	roughness = max(roughness, MIN_ROUGHNESS);
	float3 envSample = SpecularIBLMap.SampleLevel(BasicSampler, viewRefl, roughness * (SpecIBLTotalMipLevels - 1)).rgb;


	//return float4(SpecularIBLMap.Sample(ClampSampler, input.normal).rgb, 1);
	// Determine the reflectivity based on viewing angle
	// using the Schlick approximation of the Fresnel term
	float fresnel = SimpleFresnel(input.normal, -dirToPixel, 0.04f);
	return float4(lerp(sceneColor, envSample, fresnel), 1);

	//// Total color for this pixel
	//float3 totalDirectLight = float3(0, 0, 0);

	//// Loop through all lights this frame
	//for (int i = 0; i < LightCount; i++)
	//{
	//	// Which kind of light?
	//	switch (Lights[i].Type)
	//	{
	//	case LIGHT_TYPE_DIRECTIONAL:
	//		totalDirectLight += DirLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
	//		break;

	//	case LIGHT_TYPE_POINT:
	//		totalDirectLight += PointLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
	//		break;

	//	case LIGHT_TYPE_SPOT:
	//		totalDirectLight += SpotLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
	//		break;
	//	}
	//}

	//// Calculate requisite reflection vectors
	//float3 viewToCam = normalize(CameraPosition - input.worldPos);
	//float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	//float NdotV = saturate(dot(input.normal, viewToCam));

	//// Indirect lighting
	//float3 indirectDiffuse = IndirectDiffuse(IrradianceIBLMap, BasicSampler, input.normal);
	//float3 indirectSpecular = IndirectSpecular(
	//	SpecularIBLMap, SpecIBLTotalMipLevels,
	//	BrdfLookUpMap, ClampSampler, // MUST use the clamp sampler here!
	//	viewRefl, NdotV,
	//	roughness, specColor);

	//// Balance indirect diff/spec
	//float3 balancedIndirectDiff = DiffuseEnergyConserve(indirectDiffuse, indirectSpecular, metal) * surfaceColor.rgb;

	//// Multiple render target output
	//float gammaPower = 1.0f / 2.2f;
	//PS_Output output;
	//output.colorNoAmbient = float4(pow(totalDirectLight + indirectSpecular, gammaPower), 1); // Gamma correction
	//output.ambientColor = float4(pow(balancedIndirectDiff, gammaPower), 1); // Gamma correction
	//output.normals = float4(input.normal * 0.5f + 0.5f, 1);
	//output.depths = input.screenPosition.z;
	//return output;

	
}