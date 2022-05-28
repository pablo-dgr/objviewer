struct VsInput
{
    float3 position: POSITION;
    matrix modelMat: MATRIX;
};

cbuffer Data : register(b0)
{
    matrix projViewMat;
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
    matrix xformMat = mul(projViewMat, input.modelMat);
    output.position = mul(xformMat, float4(input.position.xyz, 1.0f));
    output.color = color;
    return output;
}