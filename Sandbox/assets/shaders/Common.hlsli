struct VSInput
{
    float3 position : POSITION;
    float3 normal: NORMAL;
    float3 tangent: TANGENT;
    float3 binormal : BINORMAL;
    float2 uv: UV;
};

struct Light
{
    float4 Position;
    // ( 16 bytes )
    float3 Color;
    float Intensity;
    // ( 16 bytes )
    float Range;
    float3 _padding;
    // ( 16 bytes )
};


// float CalculateAttenuation(Light light, float distance)
// {
//     return 1.0f - smoothstep( light.Range * 0.75f, light.Range, distance );
// }

float3 CalculateDiffuse(Light light, float3 L, float3 N)
{
    float NdotL = max(dot(N, L), 0);

    return light.Color * NdotL;
}

float3 CalculateSpecular(Light light, float3 V, float3 L, float3 N, float specularPower)
{
    float3 R = normalize(reflect(-L, N));
    float RdotV = max(dot(R, V), 0);
    float factor = max(pow(RdotV, specularPower), 0);
    return light.Color * factor;
}

float4 MipDebugColor(uint mip) {
    // Colors generated by : https://mokole.com/palette.html
    // Converted with : https://corecoding.com/utilities/rgb-or-hex-to-float.php

    switch (mip) 
    {
        case  0: return float4(0.184, 0.31, 0.31, 1);   // #2f4f4f
        case  1: return float4(0.498, 0, 0, 1);         // #7f0000
        case  2: return float4(0.502, 0.502, 0, 1);     // #808000
        case  3: return float4(0, 0, 0.502, 1);         // #000080
        case  4: return float4(0, 0.808, 0.82, 1);      // #00ced1
        case  5: return float4(1, 0.549, 0, 1);         // #ff8c00
        case  6: return float4(1, 1, 0, 1);             // #ffff00
        case  7: return float4(0, 1, 0, 1);             // #00ff00
        case  8: return float4(0, 0, 1, 1);             // #0000ff
        case  9: return float4(1, 0, 1, 1);             // #ff00ff
        case 10: return float4(0.392, 0.584, 0.929, 1); // #6495ed
        case 11: return float4(1, 0.078, 0.576, 1);     // #ff1493
        case 12: return float4(0.596, 0.984, 0.596, 1); // #98fb98
        case 13: return float4(1, 0.714, 0.757, 1);     // #ffb6c1
        default: return float4(1, 1, 1, 1);
    }
}