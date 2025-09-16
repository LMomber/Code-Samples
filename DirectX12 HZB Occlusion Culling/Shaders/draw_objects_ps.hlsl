struct PixelShaderInput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

struct PixelShaderOutput
{
    float4 Color : SV_Target;
};

PixelShaderOutput main(PixelShaderInput IN)
{
    PixelShaderOutput OUT;
    
    OUT.Color = IN.Color;
    return OUT;
}