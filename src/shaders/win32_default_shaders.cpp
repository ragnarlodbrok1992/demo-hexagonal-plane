#ifndef _H_WIN32_DEFAULT_SHADERS
#define _H_WIN32_DEFAULT_SHADERS

const char* win32_shader = R"(
    cbuffer ConstantBuffer : register(b0)
    {
        matrix world;
        matrix view;
        matrix projection;
    };

    struct VS_INPUT
    {
        float4 pos : POSITION;
        float4 color : COLOR;
    };

    struct PS_INPUT
    {
        float4 pos : SV_POSITION;
        float4 color : COLOR;
    };

    PS_INPUT VSMain(VS_INPUT input)
    {
        PS_INPUT output;
        // The matrices are transposed in C++, making them column-major.
        // We multiply in reverse order: P * V * W * v
        float4 pos = mul(input.pos, world);
        pos = mul(pos, view);
        pos = mul(pos, projection);
        output.pos = pos;
        output.color = input.color;
        return output;
    }

    float4 PSMain(PS_INPUT input) : SV_Target
    {
        return input.color;
    }
)";

#endif /* _H_WIN32_DEFAULT_SHADERS */

