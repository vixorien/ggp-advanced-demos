
#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	float3 viewVector;
	int ssaoEnabled;
	int ssaoOutputOnly;
	int ssrEnabled;
	int ssrOutputOnly;
	float2 pixelSize;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D SceneDirectLight		: register(t0);
Texture2D SceneIndirectSpecular	: register(t1);
Texture2D SceneAmbient			: register(t2);
Texture2D SSAOBlur				: register(t3);
Texture2D SSR					: register(t4);
Texture2D SSRBlur				: register(t5);
Texture2D SpecularColorRoughness : register(t6);
Texture2D BRDFLookUp			: register(t7);
Texture2D Normals				: register(t8);
SamplerState BasicSampler		: register(s0);


//float3 ApplyPBRToReflection(float roughness, float3 normal, float3 view, float3 specColor, float3 reflectionColor)
//{
//	// Calculate half of the split-sum approx
//	float NdotV = saturate(dot(normal, view));
//	float2 indirectBRDF = BRDFLookUp.Sample(BasicSampler, float2(NdotV, roughness)).rg;
//	float3 indSpecFresnel = specColor * indirectBRDF.x + indirectBRDF.y;
//
//	// Adjust environment sample by fresnel
//	return reflectionColor * indSpecFresnel;
//}

float4 main(VertexToPixel input) : SV_TARGET
{
	// Early out for SSAO only
	float ao = SSAOBlur.Sample(BasicSampler, input.uv).r;
	if (ssaoOutputOnly)
		return float4(ao.rrr, 1);

	// Any SSAO?
	if (!ssaoEnabled)
		ao = 1.0f;

	// Early out for SSR only
	float4 ssr = SSR.Sample(BasicSampler, input.uv);
	if (ssrOutputOnly)
		return float4(ssr.rgb, 1);

	// Sample everything else
	float3 colorDirect = SceneDirectLight.Sample(BasicSampler, input.uv).rgb;
	float4 indirectSpecularMetal = SceneIndirectSpecular.Sample(BasicSampler, input.uv);
	float3 colorIndirect = indirectSpecularMetal.rgb;
	float metal = indirectSpecularMetal.a;
	float3 colorAmbient = SceneAmbient.Sample(BasicSampler, input.uv).rgb;
	float3 normal = Normals.Sample(BasicSampler, input.uv).rgb;
	float4 ssrBlur = SSRBlur.Sample(BasicSampler, input.uv);
	float4 specRough = SpecularColorRoughness.Sample(BasicSampler, input.uv);
	float3 specColor = specRough.rgb;
	float roughness = specRough.a;

	// Adjust SSR blur based on roughness
	//ssrBlur.rgb *= roughness;
	//ssr.rgb = lerp(ssr.rgb, ssrBlur.rgb, roughness);
	//ssr.a = lerp(ssr.a, ssrBlur.a, roughness);

	// Combine indirect lighting render and ssr
	colorIndirect.rgb = ssrEnabled ? lerp(colorIndirect.rgb, ssr.rgb, ssr.a) : colorIndirect.rgb;

	// Total everything up
	float3 indirectTotal = colorIndirect + DiffuseEnergyConserve(colorAmbient, colorIndirect, metal);
	float3 totalColor = colorAmbient * ao + colorDirect + colorIndirect;
	return float4(pow(totalColor, 1.0f / 2.2f), 1);
	
}