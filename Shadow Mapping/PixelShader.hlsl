
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

	// The number of mip levels in the specular IBL map (only used for PBR)
	int SpecIBLTotalMipLevels;

	// Ambient color for the environment
	float3 AmbientNonPBR;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float4 Color;

	// Material information
	float Shininess;
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
	float4 posForShadow		: SHADOWPOS;
};

struct PS_Output
{
	float4 colorNoAmbient	: SV_TARGET0;
	float4 ambientColor		: SV_TARGET1;
	float4 normals			: SV_TARGET2;
	float depths			: SV_TARGET3;
};

// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
Texture2D ShadowMap				: register(t3);
SamplerState BasicSampler		: register(s0);
SamplerComparisonState ShadowSampler : register(s2); // Skipping s1 as some shaders use that for Clamp


// Entry point for this pixel shader
PS_Output main(VertexToPixel input)
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Normal mapping
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);
	
	// Treating roughness as a pseduo-spec map here, so applying it as
	// a modifier to the overall shininess value of the material
	float roughness = RoughnessTexture.Sample(BasicSampler, input.uv).r;
	float specPower = max(Shininess * (1.0f - roughness), 0.01f); // Ensure we never hit 0
	
	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * Color.rgb;

	// SHADOW MAPPING --------------------------------
	// Note: This is only for a SINGLE light!  If you want multiple lights to cast shadows,
	// you need to do all of this multiple times IN THIS SHADER.
	float2 shadowUV = input.posForShadow.xy / input.posForShadow.w * 0.5f + 0.5f;
	shadowUV.y = 1.0f - shadowUV.y;

	// Calculate this pixel's depth from the light
	float depthFromLight = input.posForShadow.z / input.posForShadow.w;

	// Sample the shadow map using a comparison sampler, which
	// will compare the depth from the light and the value in the shadow map
	// Note: This is applied below, after we calc our DIRECTIONAL LIGHT
	float shadowAmount = ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowUV, depthFromLight);


	// Total color for this pixel
	float3 totalDirectLight = float3(0,0,0);

	// Loop through all lights this frame
	for(int i = 0; i < LightCount; i++)
	{
		// Which kind of light?
		switch (Lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			float3 dirLightResult = DirLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			
			// Apply the directional light result, scaled by the shadow mapping
			//   Note: This demo really only has one shadow map, so this
			//   will only be correct for THE FIRST DIRECTIONAL LIGHT
			totalDirectLight += dirLightResult * (Lights[i].CastsShadows ? shadowAmount : 1.0f);
			break;

		case LIGHT_TYPE_POINT:
			totalDirectLight += PointLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			break;

		case LIGHT_TYPE_SPOT:
			totalDirectLight += SpotLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			break;
		}
	}

	// Handle ambient
	float3 ambient = surfaceColor.rgb * AmbientNonPBR;

	// Multiple render target output
	PS_Output output;
	output.colorNoAmbient	= float4(totalDirectLight, 1); // No gamma correction yet!
	output.ambientColor		= float4(ambient, 1);
	output.normals			= float4(input.normal * 0.5f + 0.5f, 1);
	output.depths			= input.screenPosition.z;
	return output;
}