struct VsInput
{
    float3 position: POSITION;
    matrix xformMat: MATRIX;
    float2 texCoords[6]: TEXCOORD;
};

cbuffer Data : register(b0)
{
    float4 color;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float4 color: COLOR;
    float2 texCoord: TEXCOORD;
};

VsOutput main(VsInput input, unsigned int vertexId : SV_VertexID)
{
    VsOutput output;
    output.position = mul(input.xformMat, float4(input.position.xyz, 1.0f));
    output.color = color;
    output.texCoord = input.texCoords[vertexId];
    return output;
}