#ifndef __GGP_COMPUTE_HELPERS__
#define __GGP_COMPUTE_HELPERS__

#define FLUID_COMPUTE_THREADS_PER_AXIS 8


float3 PixelIndexToUVW(float3 index, int gridSizeX, int gridSizeY, int gridSizeZ)
{
	// Note: Must account for half-pixel offset!
	return float3(
		(index.x + 0.5f) / gridSizeX,
		(index.y + 0.5f) / gridSizeY,
		(index.z + 0.5f) / gridSizeZ);
}

uint3 UVWToPixelIndex(float3 uvw, float3 sizes)
{
	return (uint3)floor(uvw * (sizes - 1));
}

uint3 GetLeftIndex(uint3 index)
{
	// Make sure we don't go "negative", since
	// index is an unsigned int!
	index.x = (index.x == 0 ? index.x : index.x - 1);
	return index;
}

uint3 GetRightIndex(uint3 index, int gridSizeX)
{
	// Clamp to the grid size
	index.x = min(index.x + 1, gridSizeX - 1);
	return index;
}

uint3 GetDownIndex(uint3 index)
{
	// Make sure we don't go "negative", since
	// index is an unsigned int!
	index.y = (index.y == 0 ? index.y : index.y - 1);
	return index;
}

uint3 GetUpIndex(uint3 index, int gridSizeY)
{
	// Clamp to the grid size
	index.y = min(index.y + 1, gridSizeY - 1);
	return index;
}

uint3 GetBackIndex(uint3 index)
{
	// Make sure we don't go "negative", since
	// index is an unsigned int!
	index.z = (index.z == 0 ? index.z : index.z - 1);
	return index;
}

uint3 GetForwardIndex(uint3 index, int gridSizeZ)
{
	// Clamp to the grid size
	index.z = min(index.z + 1, gridSizeZ - 1);
	return index;
}

uint3 GetOffsetIndex(int3 index, int3 sizes, int xOff, int yOff, int zOff)
{
	index += int3(xOff, yOff, zOff);
	index = min(max(index, 0), sizes - 1);
	return index;
}

// Tri-cubic interpolation
// From: https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
// Converted to HLSL below
float4 TricubicInterpolation(Texture3D tex, SamplerState samp, float3 uvw)
{
	// shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
	uint width, height, depth;
	tex.GetDimensions(width, height, depth);
	float3 nrOfVoxels = float3(width, height, depth);
	float3 coord_grid = uvw * nrOfVoxels - 0.5;
	float3 index = floor(coord_grid);
	float3 fraction = coord_grid - index;
	float3 one_frac = 1.0 - fraction;

	float3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
	float3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
	float3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
	float3 w3 = 1.0/6.0 * fraction*fraction*fraction;

	float3 g0 = w0 + w1;
	float3 g1 = w2 + w3;
	float3 mult = 1.0 / nrOfVoxels;
	float3 h0 = mult * ((w1 / g0) - 0.5 + index);  //h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
	float3 h1 = mult * ((w3 / g1) + 1.5 + index);  //h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]

	// fetch the eight linear interpolations
	// weighting and fetching is interleaved for performance and stability reasons
	float4 tex000 = tex.Sample(samp, h0);
	float4 tex100 = tex.Sample(samp, float3(h1.x, h0.y, h0.z));
	tex000 = lerp(tex100, tex000, g0.x);  //weigh along the x-direction
	float4 tex010 = tex.Sample(samp, float3(h0.x, h1.y, h0.z));
	float4 tex110 = tex.Sample(samp, float3(h1.x, h1.y, h0.z));
	tex010 = lerp(tex110, tex010, g0.x);  //weigh along the x-direction
	tex000 = lerp(tex010, tex000, g0.y);  //weigh along the y-direction
	float4 tex001 = tex.Sample(samp, float3(h0.x, h0.y, h1.z));
	float4 tex101 = tex.Sample(samp, float3(h1.x, h0.y, h1.z));
	tex001 = lerp(tex101, tex001, g0.x);  //weigh along the x-direction
	float4 tex011 = tex.Sample(samp, float3(h0.x, h1.y, h1.z));
	float4 tex111 = tex.Sample(samp, h1);
	tex011 = lerp(tex111, tex011, g0.x);  //weigh along the x-direction
	tex001 = lerp(tex011, tex001, g0.y);  //weigh along the y-direction

	return lerp(tex001, tex000, g0.z);  //weigh along the z-direction
}

#endif