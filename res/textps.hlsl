struct PsInput
{
    float4 position : SV_POSITION;
    float4 color: COLOR;
    float2 texCoord: TEXCOORD;
};

Texture2D<float> bakedFontTex : register(t0);
SamplerState fontTexSampler : register(s0);

float4 main(PsInput input) : SV_TARGET
{
    float alphaSample = bakedFontTex.Sample(fontTexSampler, input.texCoord);
    return float4(input.color.xyz, alphaSample);
}