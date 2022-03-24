
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
	matrix projMatrix;
	float2 screenSize;
	int useRefraction;
	int useRefractionSilhouette;
	int refractionFromNormalMap;
	float refractionScale;
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
Texture2D NormalTexture			: register(t0);
Texture2D RoughnessTexture		: register(t1);

// Environment map for reflections
TextureCube EnvironmentMap		: register(t2);

// Refraction requirement
Texture2D ScreenPixels			: register(t3);
Texture2D RefractionSilhouette	: register(t4);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

// Fresnel term - Schlick approx.
float SimpleFresnel(float3 n, float3 v, float f0)
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
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);

	// The actual screen UV and refraction offset UV
	float2 screenUV = input.screenPosition.xy / screenSize;
	float2 offsetUV = float2(0, 0);

	// Which kind of refraction?
	if (refractionFromNormalMap)
	{
		// Use the normal map itself
		offsetUV = NormalTexture.Sample(BasicSampler, input.uv).xy * 2 - 1;
	}
	else
	{
		// Convert the world space normal to view space
		float3 normalViewSpace = mul((float3x3)viewMatrix, input.normal);

		// Use overall surface normal (with normal mapping applied)
		offsetUV = normalViewSpace.xy;
	}

	// Flip the offset's V upside down due to world <--> texture space difference
	offsetUV.y *= -1;

	// Final refracted UV (where to pull the pixel color)
	// If not using refraction at all, just use the screen UV itself
	float2 refractedUV = useRefraction ? screenUV + offsetUV * refractionScale : screenUV;

	// Get the depth at the offset and verify its valid
	float silhouette = RefractionSilhouette.Sample(ClampSampler, refractedUV).r;
	if (useRefractionSilhouette && silhouette < 1)
	{
		// Invalid spot for the offset so default to THIS pixel's UV for the "refraction"
		refractedUV = screenUV;
	}

	// Get the color at the (now verified) offset UV
	float3 sceneColor = pow(ScreenPixels.Sample(ClampSampler, refractedUV).rgb, 2.2f); // Un-gamma correct

	// Get reflections
	float3 viewToCam = normalize(CameraPosition - input.worldPos);
	float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	float3 envSample = EnvironmentMap.Sample(BasicSampler, viewRefl).rgb;

	// Determine the reflectivity based on viewing angle
	// using the Schlick approximation of the Fresnel term
	float fresnel = SimpleFresnel(input.normal, viewToCam, F0_NON_METAL);
	return float4(pow(lerp(sceneColor, envSample, fresnel), 1.0f / 2.2f), 1); // Re-gamma correct after linear interpolation

}