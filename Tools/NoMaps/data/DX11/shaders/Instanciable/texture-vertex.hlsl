/////////////
// GLOBALS //
/////////////
cbuffer MatrixBuffer : register(b0)
{
    matrix wvpMatrix;
    matrix wldMatrix;
    float3 eyeVector;
};

//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
    uint   instid   : SV_InstanceID;
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;

    float4 transformu : TRANSFORMU;
    float4 transformv : TRANSFORMV;
    float4 transformw : TRANSFORMW;
    float4 transformt : TRANSFORMT;
};

struct PixelInputType
{
    float4 position  : SV_Position;
    float2 texcoord  : TEXCOORD;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType RenderSceneVS(VertexInputType input)
{
    PixelInputType output;
		
    input.position.w = 1.0f;

//  output.position  = mul(input.position, float4x4(transformu, transformv, transformw, transformt));
    output.position  = mul(input.position, wvpMatrix);
    output.texcoord  = input.texcoord;

    return output;
}