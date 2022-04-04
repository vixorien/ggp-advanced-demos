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

#endif