struct VsInput
{
    float3 position: POSITION;
};

cbuffer Data : register(b0)
{
    matrix xformMat;
};

float4 main(VsInput input) : SV_POSITION
{
    return mul(xformMat, float4(input.position.xyz, 1.0f));
}