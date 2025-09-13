#ifndef _H_ON_INIT_COMPILE_SHADERS
#define _H_ON_INIT_COMPILE_SHADERS

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

void onInitCompileShaders(win32_Renderer& renderer, win32_Shaders& shaders, const char* shader_source) {
  // Shaders debug layer - if enabled there is no optimization
#if defined(DEBUG_DIRECTX)
    #pragma message("DEBUG_DIRECTX defined - setting up shaders compile flags to DEBUG/SKIP_OPTIMIZATION.")
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // Resource declaration part
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
      // Stuff here depends on what shader we are compiling I think
      // TODO(moliwa): We should do something about it in more general way, but let's leave it for now
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } // Why is 12 here? is it 3x4bytes or something like that?
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    D3D12_BLEND_DESC blendDesc = {};

    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };

    // Procedures part
    ThrowIfFailed(D3DCompile(shader_source, strlen(shader_source), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &shaders.vertexShader, nullptr));
    ThrowIfFailed(D3DCompile(shader_source, strlen(shader_source), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &shaders.pixelShader, nullptr));

    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = renderer.root_signature.Get();
    psoDesc.VS = { reinterpret_cast<UINT8*>(shaders.vertexShader->GetBufferPointer()), shaders.vertexShader->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<UINT8*>(shaders.pixelShader->GetBufferPointer()), shaders.pixelShader->GetBufferSize() };

    // TODO(ragnar): Cut this code somehow to logical groups
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    psoDesc.RasterizerState = rasterizerDesc;
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(renderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderer.pipeline_state)));
    ThrowIfFailed(renderer.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer.command_allocator.Get(), renderer.pipeline_state.Get(), IID_PPV_ARGS(&renderer.command_list)));
    ThrowIfFailed(renderer.command_list->Close());

    ThrowIfFailed(renderer.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer.command_allocator.Get(), renderer.pipeline_state.Get(), IID_PPV_ARGS(&renderer.command_list)));
    ThrowIfFailed(renderer.command_list->Close());

    ThrowIfFailed(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer.fence)));
    g_fenceValue = 1;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (g_fenceEvent == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

#endif /* _H_ON_INIT_COMPILE_SHADERS */

