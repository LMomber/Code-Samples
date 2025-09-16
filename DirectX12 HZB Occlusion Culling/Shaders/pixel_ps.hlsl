struct PixelShaderInput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

struct PixelShaderOutput
{
    float4 Color : SV_Target;
    float Depth : SV_Depth;
};

PixelShaderOutput main(PixelShaderInput IN)
{
    PixelShaderOutput OUT;
    
    OUT.Color = IN.Color;
    OUT.Depth = 1 - IN.Position.z / IN.Position.w * 10;
    return OUT;
}