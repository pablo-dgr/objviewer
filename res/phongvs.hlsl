struct VsInput
{
    float3 position: POSITION;
    float3 normal: NORMAL;
};

cbuffer Data : register(b0)
{
    matrix projViewMat;
    matrix modelMat;
    matrix normalMat;
    float4 color;
    float3 lightPosition;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION;
    float4 color: COLOR;
    float3 normal : NORMAL;
    float3 lightPosition : LIGHT;
};

VsOutput main(VsInput input)
{
    VsOutput output;
    matrix xFormMat = mul(projViewMat, modelMat);
    output.position = mul(xFormMat, float4(input.position.xyz, 1.0f));
    output.worldPosition = mul(modelMat, float4(input.position.xyz, 1.0f));
    output.color = color;
    float4 psNormal = mul(normalMat, float4(input.normal, 1.0f));
    output.normal = psNormal.xyz;
    output.lightPosition = lightPosition;
    return output;
}