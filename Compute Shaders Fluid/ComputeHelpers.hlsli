#ifndef __GGP_COMPUTE_HELPERS__
#define __GGP_COMPUTE_HELPERS__

#define FLUID_COMPUTE_THREADS_PER_AXIS 8


float3 PixelIndexToUVW(uint3 index, int gridSizeX, int gridSizeY, int gridSizeZ)
{
	// Note: Must account for half-pixel offset!
	return float3(
		(index.x + 0.5f) / gridSizeX,
		(index.y + 0.5f) / gridSizeY,
		(index.z + 0.5f) / gridSizeZ);
}

#endif