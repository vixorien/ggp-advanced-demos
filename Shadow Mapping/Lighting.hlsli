// Include guard
#ifndef _LIGHTING_HLSL
#define _LIGHTING_HLSL

static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float PI_OVER_2 = PI / 2.0f;

#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

struct Light
{
	int		Type;
	float3	Direction;	// 16 bytes

	float	Range;
	float3	Position;	// 32 bytes

	float	Intensity;
	float3	Color;		// 48 bytes

	float	SpotFalloff;
	int		CastsShadows;
	float2	Padding;	// 64 bytes
};

// === UTILITY FUNCTIONS ============================================

// Basic sample and unpack
float3 SampleAndUnpackNormalMap(Texture2D map, SamplerState samp, float2 uv)
{
	return map.Sample(samp, uv).rgb * 2.0f - 1.0f;
}

// Handle converting tangent-space normal map to world space normal
float3 NormalMapping(Texture2D map, SamplerState samp, float2 uv, float3 normal, float3 tangent)
{
	// Grab the normal from the map
	float3 normalFromMap = SampleAndUnpackNormalMap(map, samp, uv);

	// Gather the required vectors for converting the normal
	float3 N = normal;
	float3 T = normalize(tangent - N * dot(tangent, N));
	float3 B = cross(T, N);

	// Create the 3x3 matrix to convert from TANGENT-SPACE normals to WORLD-SPACE normals
	float3x3 TBN = float3x3(T, B, N);

	// Adjust the normal from the map and simply use the results
	return normalize(mul(normalFromMap, TBN));
}

// Range-based attenuation function
float Attenuate(Light light, float3 worldPos)
{
	float dist = distance(light.Position, worldPos);

	// Ranged-based attenuation
	float att = saturate(1.0f - (dist * dist / (light.Range * light.Range)));

	// Soft falloff
	return att * att;
}

// Create world space position from a depth, uv and inverse matrices
float3 WorldSpaceFromDepth(float depth, float2 uv, matrix invViewMatrix, matrix invProjMatrix)
{
	// Back to NDCs
	uv = uv * 2.0f - 1.0f;
	uv.y *= -1.0f; // Flip y due to UV <--> NDC 
	float4 screenPos = float4(uv, depth, 1.0f);

	// Back to view space first
	float4 viewPos = mul(invProjMatrix, screenPos);
	viewPos /= viewPos.w;

	// Back to world space
	return mul(invViewMatrix, viewPos).xyz;
}



// === BASIC LIGHTING ===============================================

// Lambert diffuse BRDF
float Diffuse(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}

// Blinn-Phong (specular) BRDF
float SpecularBlinnPhong(float3 normal, float3 dirToLight, float3 toCamera, float shininess)
{
	// Calculate halfway vector
	float3 halfwayVector = normalize(dirToLight + toCamera);

	// Compare halflway vector and normal and raise to a power
	return shininess == 0 ? 0.0f : pow(max(dot(halfwayVector, normal), 0), shininess);
}




// === LIGHT TYPES FOR BASIC LIGHTING ===============================


float3 DirLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Get normalize direction to the light
	float3 toLight = normalize(-light.Direction);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float diff = Diffuse(normal, toLight);
	float spec = SpecularBlinnPhong(normal, toLight, toCam, shininess);

	// Combine
	return (diff * surfaceColor + spec) * light.Intensity * light.Color;
}


float3 PointLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Calc light direction
	float3 toLight = normalize(light.Position - worldPos);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float atten = Attenuate(light, worldPos);
	float diff = Diffuse(normal, toLight);
	float spec = SpecularBlinnPhong(normal, toLight, toCam, shininess);

	// Combine
	return (diff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}


float3 SpotLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Calculate the spot falloff
	float3 toLight = normalize(light.Position - worldPos);
	float penumbra = pow(saturate(dot(-toLight, light.Direction)), light.SpotFalloff);
	
	// Combine with the point light calculation
	// Note: This could be optimized a bit
	return PointLight(light, normal, worldPos, camPos, shininess, surfaceColor) * penumbra;
}


// === PHYSICALLY BASED LIGHTING ====================================

// PBR Constants:

// The fresnel value for non-metals (dielectrics)
// Page 9: "F0 of nonmetals is now a constant 0.04"
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// Also slide 65 of http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
static const float F0_NON_METAL = 0.04f;

// Need a minimum roughness for when spec distribution function denominator goes to zero
static const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal

// Lambert diffuse BRDF - Same as the basic lighting!
float DiffusePBR(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}



// GGX (Trowbridge-Reitz)
//
// a - Roughness
// h - Half vector
// n - Normal
// 
// D(h, n) = a^2 / pi * ((n dot h)^2 * (a^2 - 1) + 1)^2
float SpecDistribution(float3 n, float3 h, float roughness)
{
	// Pre-calculations
	float NdotH = saturate(dot(n, h));
	float NdotH2 = NdotH * NdotH;
	float a = roughness * roughness;
	float a2 = max(a * a, MIN_ROUGHNESS); // Applied after remap!

	// ((n dot h)^2 * (a^2 - 1) + 1)
	float denomToSquare = NdotH2 * (a2 - 1) + 1;
	// Can go to zero if roughness is 0 and NdotH is 1

	// Final value
	return a2 / (PI * denomToSquare * denomToSquare);
}



// Fresnel term - Schlick approx.
// 
// v - View vector
// h - Half vector
// f0 - Value when l = n (full specular color)
//
// F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5
float3 Fresnel(float3 v, float3 h, float3 f0)
{
	// Pre-calculations
	float VdotH = saturate(dot(v, h));

	// Final value
	return f0 + (1 - f0) * pow(1 - VdotH, 5);
}



// Geometric Shadowing - Schlick-GGX (based on Schlick-Beckmann)
// - k is remapped to a / 2, roughness remapped to (r+1)/2
//
// n - Normal
// v - View vector
//
// G(l,v,h)
float GeometricShadowing(float3 n, float3 v, float roughness)
{
	// End result of remapping:
	float k = pow(roughness + 1, 2) / 8.0f;
	float NdotV = saturate(dot(n, v));

	// Final value
	return NdotV / (NdotV * (1 - k) + k);
}



// Microfacet BRDF (Specular)
//
// f(l,v) = D(h)F(v,h)G(l,v,h) / 4(n dot l)(n dot v)
// - part of the denominator are canceled out by numerator (see below)
//
// D() - Spec Dist - Trowbridge-Reitz (GGX)
// F() - Fresnel - Schlick approx
// G() - Geometric Shadowing - Schlick-GGX
float3 MicrofacetBRDF(float3 n, float3 l, float3 v, float roughness, float metalness, float3 specColor)
{
	// Other vectors
	float3 h = normalize(v + l);

	// Grab various functions
	float D = SpecDistribution(n, h, roughness);
	float3 F = Fresnel(v, h, specColor); // This ranges from F0_NON_METAL to actual specColor based on metalness
	float G = GeometricShadowing(n, v, roughness) * GeometricShadowing(n, l, roughness);

	// Final formula
	// Denominator dot products partially canceled by G()!
	// See page 16: http://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
	return (D * F * G) / (4 * max(dot(n,v), dot(n,l)));
}

// Calculates diffuse amount based on energy conservation
//
// diffuse - Diffuse amount
// specular - Specular color (including light color)
// metalness - surface metalness amount
//
// Metals should have an albedo of (0,0,0)...mostly
// See slide 65: http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
float3 DiffuseEnergyConserve(float3 diffuse, float3 specular, float metalness)
{
	return diffuse * ((1 - saturate(specular)) * (1 - metalness));
}


// === LIGHT TYPES FOR PBR LIGHTING =================================


float3 DirLightPBR(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float3 specularColor)
{
	// Get normalize direction to the light
	float3 toLight = normalize(-light.Direction);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float diff = DiffusePBR(normal, toLight);
	float3 spec = MicrofacetBRDF(normal, toLight, toCam, roughness, metalness, specularColor);

	// Calculate diffuse with energy conservation
	// (Reflected light doesn't get diffused)
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Combine amount with 
	return (balancedDiff * surfaceColor + spec) * light.Intensity * light.Color;
}


float3 PointLightPBR(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float3 specularColor)
{
	// Calc light direction
	float3 toLight = normalize(light.Position - worldPos);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float atten = Attenuate(light, worldPos);
	float diff = DiffusePBR(normal, toLight);
	float3 spec = MicrofacetBRDF(normal, toLight, toCam, roughness, metalness, specularColor);

	// Calculate diffuse with energy conservation
	// (Reflected light doesn't diffuse)
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Combine
	return (balancedDiff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}


float3 SpotLightPBR(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float3 specularColor)
{
	// Calculate the spot falloff
	float3 toLight = normalize(light.Position - worldPos);
	float penumbra = pow(saturate(dot(-toLight, light.Direction)), light.SpotFalloff);

	// Combine with the point light calculation
	// Note: This could be optimized a bit
	return PointLightPBR(light, normal, worldPos, camPos, roughness, metalness, surfaceColor, specularColor) * penumbra;
}


// === INDIRECT PBR (IBL) ===========================================

// Indirect diffuse irradiance for the scene
// 
// Uses an irradiance map for indirect diffuse lighting
//
// irrMap			- The irradiance cube map
// samp				- Sampler to use
// direction		- Direction for sampling cube map
//
float3 IndirectDiffuse(TextureCube irrMap, SamplerState samp, float3 direction)
{
	// Sample in the specified direction - the irradiance map
	// is a pre-computed cube map which represents light
	// coming into this pixel from a particular hemisphere
	float3 diff = irrMap.SampleLevel(samp, direction, 0).rgb;
	return pow(abs(diff), 2.2);

}


// Indirect specular (environment reflections)
//
// Uses a pre-convolved cube map and a pre-calculated view-angle/roughness BRDF texture
// to calculate the final "blurred" environment reflection color for this pixel
//
// envMap			- Pre-convolved environment map w/ mip levels
// mips				- Number of mips in the environment map
// brdfLookUp		- Pre-calc'd environment BRDF lookup texture
// samp				- Sampler to use (MUST BE CLAMP ADDRESS MODE for envBRDF to work right!!!)
// viewRefl			- View reflection direction at this pixel
// NdotV			- Dot(normal, view vector) at this pixel
// roughness		- Roughness of this pixel
// specColor		- Specular color of this pixel (already taking into account metalness)
//
float3 IndirectSpecular(TextureCube envMap, int mips, Texture2D brdfLookUp, SamplerState samp, float3 viewRefl, float NdotV, float roughness, float3 specColor)
{
	// Ensure roughness isn't zero
	roughness = max(roughness, MIN_ROUGHNESS);

	// Calculate half of the split-sum approx (this texture is not gamma-corrected, as it just holds raw data)
	float2 indirectBRDF = brdfLookUp.Sample(samp, float2(NdotV, roughness)).rg;
	float3 indSpecFresnel = specColor * indirectBRDF.x + indirectBRDF.y; // Spec color is f0

	// Sample the convolved environment map (other half of split-sum)
	float3 envSample = envMap.SampleLevel(samp, viewRefl, roughness * (mips - 1)).rgb;

	// Adjust environment sample by fresnel
	return pow(abs(envSample), 2.2) * indSpecFresnel;
}


// === UTILITY FUNCTIONS for Indirect PBR Pre-Calculations ====================


// Part of the Hammersley 2D sampling function.  More info here:
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// This function is useful for computing numbers in the Van der Corput sequence
//
// Ok, so this looks like voodoo magic, and sort of is (at the bit level).
// 
// The entire point of this function is to Very Quickly, using bit math, 
// mirror the binary version of an integer across the decimal point.
//
// Or, to put it another way: turn 0101.0 (the int) into 0.1010 (the float)
//
// Here is a quick list of example inputs & outputs:
//
// Input (int)	Binary (before)		Binary (after)		Output (as a float)
//  0			 0000.0				 0.0000				 0.0
//  1			 0001.0				 0.1000				 0.5
//  2			 0010.0				 0.0100				 0.25
//  3			 0011.0				 0.1100				 0.75
//  4			 0100.0				 0.0010				 0.125
//  5			 0101.0				 0.1010				 0.625
//
// Cool!  So...why?  Given any integer, we get a float in the range [0,1), 
// and the resulting float values are fairly well distributed (rather than
// all being "bunched" up near each other).  This is GREAT if we want to sample
// some regular pixels across a large area of a texture to get an average/blur.
//
// bits - an unsigned integer value to "mirror" at the bit level
//
float radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sampling
// Useful to get some fairly-well-distributed (spread out) points on a 2D grid
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
//  - The X value of the float2 is simply i/N (current value/total values),
//     which will be evenly distributed between 0 to 1 as i goes from 0 to N
//  - The Y value will "jump around" a bit using the radicalInverse trick,
//     but still end up being fairly evenly distributed between 0 and 1
//
// i - The current value (between 0 and N)
// N - The total number of samples you'll be taking
//
float2 Hammersley2d(uint i, uint N) {
	return float2(float(i) / float(N), radicalInverse_VdC(i));
}

// Important sampling with GGX
// 
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
//
// Calculates a direction in space offset from a starting direction (N), based on
// a roughness value and a point on a 2d grid.  The 2d grid value is essentially an
// offset from the starting direction in 2d, so it needs to be translated to 3d first.
//
// Xi			- a point on a 2d grid, later converted to a 3d offset
// roughness	- the roughness of the surface, which tells us how "blurry" reflections are
// N			- The normal around which we generate this new direction
//
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
	float a = roughness * roughness;

	float Phi = 2 * PI * Xi.x;
	float CosTheta = sqrt((1 - Xi.y) / (1 + (a * a - 1) * Xi.y));
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float3 UpVector = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);

	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}



#endif