struct PsInput
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION;
    float4 color: COLOR;
    float3 normal : NORMAL;
    float3 lightPosition : LIGHT;
};

float4 main(PsInput input) : SV_TARGET
{
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float ambientScale = 0.2f;
    float3 ambient = lightColor * ambientScale;

    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(input.lightPosition - input.worldPosition.xyz);
    float lightDiff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = lightColor * lightDiff;

    float3 color = input.color.xyz * (ambient + diffuse);

    return float4(color, 1.0f);
}