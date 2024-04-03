
cbuffer ExternalData : register(b0)
{
	int MaxParticles;
}

AppendStructuredBuffer<uint> DeadList : register(u0);

[numthreads(32, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Outside range?
	if(id.x >= (uint)MaxParticles) return;

	// Add the index to the dead list
	DeadList.Append(id.x);
}