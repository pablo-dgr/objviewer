struct VsInput
{
    float3 position: POSITION;
};

float4 main(VsInput input) : SV_POSITION
{
    return float4(input.position.xyz, 1.0f);
}