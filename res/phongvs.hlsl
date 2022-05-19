struct VsInput
{
    float3 position: POSITION;
    float3 normal: NORMAL;
};

cbuffer Data : register(b0)
{
    matrix xformMat;
    float4 color;
    float3 lightPosition;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float4 color: COLOR;
    float3 normal : NORMAL;
    float3 lightPosition : LIGHT;
};

VsOutput main(VsInput input)
{
    VsOutput output;
    output.position = mul(xformMat, float4(input.position.xyz, 1.0f));
    output.color = color;
    output.normal = input.normal;
    output.lightPosition = lightPosition;
    return output;
}