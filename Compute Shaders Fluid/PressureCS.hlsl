
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			DivergenceIn	: register(t0);
Texture3D			PressureIn		: register(t1);
Texture3D			ObstaclesIn		: register(t2);
RWTexture3D<float>	PressureOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Get the divergence here
	float div = DivergenceIn[id].r;
	float pressureHere = PressureIn[id].r;

	// Indices of surrounding pixels
	uint3 idL = GetLeftIndex(id);
	uint3 idR = GetRightIndex(id, gridSizeX);
	uint3 idD = GetDownIndex(id);
	uint3 idU = GetUpIndex(id, gridSizeY);
	uint3 idB = GetBackIndex(id);
	uint3 idF = GetForwardIndex(id, gridSizeZ);

	// Pressure of surrounding pixels
	// Note: The nature of our neighbor checking here
	// will return the current cell's pressure value
	// (since we're basically clamping), which is exactly
	// what we want at the boundaries of the volume.  This
	// means we don't need to explicitly check for edges.
	float pL = PressureIn[idL].r;
	float pR = PressureIn[idR].r;
	float pD = PressureIn[idD].r;
	float pU = PressureIn[idU].r;
	float pB = PressureIn[idB].r;
	float pF = PressureIn[idF].r;

	// Use this cell's pressure for any surrounding
	// cells that contain an obstacle
	if (ObstaclesIn[idL].r > 0.0f) pL = pressureHere;
	if (ObstaclesIn[idR].r > 0.0f) pR = pressureHere;
	if (ObstaclesIn[idD].r > 0.0f) pD = pressureHere;
	if (ObstaclesIn[idU].r > 0.0f) pU = pressureHere;
	if (ObstaclesIn[idB].r > 0.0f) pB = pressureHere;
	if (ObstaclesIn[idF].r > 0.0f) pF = pressureHere;

	// Compute the pressure based on surrounding cells
	float pressure = (pL + pR + pD + pU + pB + pF - div) / 6.0f;
	PressureOut[id] = pressure;
}