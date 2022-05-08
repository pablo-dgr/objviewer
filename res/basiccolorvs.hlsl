struct VsInput
{
    float3 position: POSITION;
};

cbuffer Data : register(b0)
{
    matrix xformMat;
    float4 color;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float4 color: COLOR;
};

VsOutput main(VsInput input)
{
    VsOutput output;
    output.position = mul(xformMat, float4(input.position.xyz, 1.0f));
    output.color = color;
    return output;
}