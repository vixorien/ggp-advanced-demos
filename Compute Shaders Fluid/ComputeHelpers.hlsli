#ifndef __GGP_COMPUTE_HELPERS__
#define __GGP_COMPUTE_HELPERS__

#define FLUID_COMPUTE_THREADS_PER_AXIS 8

float3 PixelIndexToUVW(uint3 index, int gridSizeX, int gridSizeY, int gridSizeZ)
{
	return float3(
		index.x / (float)gridSizeX,
		index.y / (float)gridSizeY,
		index.z / (float)gridSizeZ);
}

#endif