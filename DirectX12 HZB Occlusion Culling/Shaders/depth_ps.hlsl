struct PixelShaderInput
{
    float4 Position : SV_Position;
};

float main(PixelShaderInput IN) : SV_Depth
{
    return 1 -IN.Position.z / IN.Position.w ;
}