
// Configuration variables - might go even before we include stuff if this stuff ends up in different files
constexpr int DISPLAY_FACTOR = 60;
constexpr int DISPLAY_WIDTH = 16 * DISPLAY_FACTOR;
constexpr int DISPLAY_HEIGHT = 9 * DISPLAY_FACTOR;


#include "renderer.cpp"
#include "win_utils.cpp"
#include "camera.cpp"
#include "input.cpp"
#include "entities/cube.cpp"
#include "shaders/win32_default_shaders.cpp"
#include "render_pipeline/on_init.cpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <cstdlib>
#include <ctime>

// Global variables
HWND g_hwnd = NULL;

// DirectX3D 12 renderer - win32 part
win32_Renderer renderer = {0};
win32_Shaders shaders   = {0};

// Pack this together? This is used for objects that are being renderer
// Entities on the screen.
Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;

D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
D3D12_INDEX_BUFFER_VIEW indexBufferView;
UINT8* constantBufferView;  // There is something called D3D12_CONSTANT_BUFFER_VIEW_DESC in d3d12.h

// Some fence stuff - TODO(moliwa): Move to renderer
HANDLE g_fenceEvent;
UINT64 g_fenceValue;

struct MVPMatrix {
    DirectX::XMMATRIX world;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
};

MVPMatrix constantBufferData;

DirectX::XMMATRIX g_worldMatrix; // Rotation stuff becomes worldMatrix TODO(moliwa): Copy it from camera?
DirectX::XMMATRIX g_viewMatrix;
DirectX::XMMATRIX g_projectionMatrix;


// Forward declarations - maybe remove them?
void onInit();
void prepareCube();
void prepareCamera();
void onUpdate();
void onRender();
void onDestroy();
void WaitForPreviousFrame();

// Engine entities - ex. Camera is engine entity - controlled by engine, but bothered by game state and shouldn't know anything out it.
static Camera camera = {};
static Input input = {};

// Game entities
static Vertex* cubeVertices = {};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_LBUTTONDOWN:
        {
            input.left_mouse_button_down = true;
            GetCursorPos(&input.last_mouse_pos);
            ScreenToClient(hwnd, &input.last_mouse_pos);
            return 0;
        }
        case WM_LBUTTONUP:
        {
            input.left_mouse_button_down = false;
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            if (input.left_mouse_button_down) {
              // TODO(moliw): Fix rotations, those suck
              // I might not understand for now how to do it properly...
                POINT currentMousePos;
                GetCursorPos(&currentMousePos);
                ScreenToClient(hwnd, &currentMousePos);

                float dx = static_cast<float>(currentMousePos.x - input.last_mouse_pos.x);
                float dy = static_cast<float>(currentMousePos.y - input.last_mouse_pos.y);

                camera.rotation_y -= dx * 0.01f;
                camera.rotation_x -= dy * 0.01f;

                input.last_mouse_pos = currentMousePos;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    srand(static_cast<unsigned int>(time(0)));
    
    cubeVertices = createDefaultCube();

    const char CLASS_NAME[] = "hexagonal-plane";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    g_hwnd = CreateWindowExA(
        0, CLASS_NAME, "DirectX 12 Learning Code...", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr
    );

    if (g_hwnd == NULL) {
        return 0;
    }

    // TODO(ragnar): Remove exception
    // TODO(ragnar): Split this calls - we don't know what fails
    try {
        onInit();
        prepareCamera();
        prepareCube();
    } catch (const std::runtime_error& e) {
        MessageBoxA(g_hwnd, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    ShowWindow(g_hwnd, nCmdShow);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            onUpdate();
            onRender();
        }
    }

    onDestroy();

    return static_cast<char>(msg.wParam);
}


void onInit() {
    // Enable the D3D12 debug layer.
#if defined(DEBUG_DIRECTX)
    {
      #pragma message("DEBUG_DIRECTX defined - setting up debug layer for DirectX3D 12.")
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
        }
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&renderer.device)));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(renderer.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer.command_queue)));

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = BUFFER_COUNT;
    swapChainDesc.Width = 1280;
    swapChainDesc.Height = 720;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        renderer.command_queue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));
    ThrowIfFailed(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&renderer.swap_chain));
    FRAME_INDEX = renderer.swap_chain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(renderer.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&renderer.rtv_heap)));
    RTV_DESCRIPTOR_SIZE = renderer.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT n = 0; n < BUFFER_COUNT; n++) {
        ThrowIfFailed(renderer.swap_chain->GetBuffer(n, IID_PPV_ARGS(&renderer.render_targets[n])));
        renderer.device->CreateRenderTargetView(renderer.render_targets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += RTV_DESCRIPTOR_SIZE;
    }

    ThrowIfFailed(renderer.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderer.command_allocator)));

    D3D12_ROOT_PARAMETER rootParameters[1];
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(renderer.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&renderer.root_signature)));

#if defined(DEBUG_DIRECTX)
    #pragma message("DEBUG_DIRECTX defined - setting up shaders compile flags to DEBUG/SKIP_OPTIMIZATION.")
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ThrowIfFailed(D3DCompile(win32_shader, strlen(win32_shader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &shaders.vertexShader, nullptr));
    ThrowIfFailed(D3DCompile(win32_shader, strlen(win32_shader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &shaders.pixelShader, nullptr));

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = renderer.root_signature.Get();
        psoDesc.VS = { reinterpret_cast<UINT8*>(shaders.vertexShader->GetBufferPointer()), shaders.vertexShader->GetBufferSize() };
        psoDesc.PS = { reinterpret_cast<UINT8*>(shaders.pixelShader->GetBufferPointer()), shaders.pixelShader->GetBufferSize() };
        
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
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

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
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

    ThrowIfFailed(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer.fence)));
    g_fenceValue = 1;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (g_fenceEvent == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void prepareCamera () {
    // Renderer initialized, below stuff is engine related
    
    g_worldMatrix = DirectX::XMMatrixIdentity();

    // Maybe we can write something about DirectX3D space coordinates?
    // It's right hand - pointed straight to our face. Thumb is y, second finger is x, first finger is z.
    // -z to our face.
    // +y up.
    // Rest is history
    DirectX::XMVECTOR eye = DirectX::XMVectorSet(2.0f, 2.0f, -2.0f, 0.0f);
    DirectX::XMVECTOR at  = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up  = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_viewMatrix = DirectX::XMMatrixLookAtLH(eye, at, up);

    // Perspective projection provided straight from microsoft vaults.
    g_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, 1280.0f / 720.0f, 0.1f, 100.0f);

    // WaitForPreviousFrame();
}

// Test function to properly render our test cube
void prepareCube() {
    // Do we end creating directx stuff above?
    // No, we are still inside onInit function
    // TODO(moliwa): Split creation of renderer and stuff for rendered entities

    const UINT vertexBufferSize = sizeof(Vertex) * DEFAULT_CUBE_VERTICES;

    D3D12_HEAP_PROPERTIES heapProps = {};
    D3D12_RESOURCE_DESC resourceDesc = {};
    D3D12_RANGE readRange = {0, 0};

    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(renderer.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)));

    UINT8* pVertexDataBegin;
    ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));

    memcpy(pVertexDataBegin, cubeVertices, vertexBufferSize);

    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vertexBufferSize;

    const UINT indexBufferSize = sizeof(cubeIndices);
    resourceDesc.Width = indexBufferSize;  // ResourceDesc is being reused here!

    ThrowIfFailed(renderer.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuffer)));

    UINT8* pIndexDataBegin;
    ThrowIfFailed(indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
    memcpy(pIndexDataBegin, cubeIndices, sizeof(cubeIndices));
    indexBuffer->Unmap(0, nullptr);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    indexBufferView.SizeInBytes = indexBufferSize;

  // We prepare something like GPU heap here and try to push data on it?
  // Key call I think are: CreateComittedResource, Map and memcpy
  // TODO(moliwa): Is there something else besides memcpy to move data?
    heapProps = {};
    resourceDesc = {};
    readRange = {0, 0};
    
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // resourceDesc.Width = 1024 * 64; // Why this sizes here??? - code generated
    // According to https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d2d12_resource_desc
    // Alignment might be one of 0, 4KB, 64KB or 4MB so I guess this here makes it like that.
    // Here we do not set this field
    resourceDesc.Width = 1024 * 4; // Why this sizes here???
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(renderer.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)));

    ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&constantBufferView)));
    memcpy(constantBufferView, &constantBufferData, sizeof(constantBufferData));
}

void onUpdate() {
    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationX(camera.rotation_x) * DirectX::XMMatrixRotationY(camera.rotation_y);
    g_worldMatrix = rotationMatrix;

    // Transpose the matrices to be column-major for the shader.
    constantBufferData.world = DirectX::XMMatrixTranspose(g_worldMatrix);
    constantBufferData.view = DirectX::XMMatrixTranspose(g_viewMatrix);
    constantBufferData.projection = DirectX::XMMatrixTranspose(g_projectionMatrix);
    
    memcpy(constantBufferView, &constantBufferData, sizeof(constantBufferData));
}

void onRender() {
    ThrowIfFailed(renderer.command_allocator->Reset());
    ThrowIfFailed(renderer.command_list->Reset(renderer.command_allocator.Get(), renderer.pipeline_state.Get()));

    renderer.command_list->SetGraphicsRootSignature(renderer.root_signature.Get());
    renderer.command_list->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT scissorRect = { 0, 0, 1280, 720 };
    renderer.command_list->RSSetViewports(1, &viewport);
    renderer.command_list->RSSetScissorRects(1, &scissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderer.render_targets[FRAME_INDEX].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderer.command_list->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += FRAME_INDEX * RTV_DESCRIPTOR_SIZE;
    renderer.command_list->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.0f, 0.0f, 0.4f, 1.0f };
    renderer.command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    renderer.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderer.command_list->IASetVertexBuffers(0, 1, &vertexBufferView);
    renderer.command_list->IASetIndexBuffer(&indexBufferView);
    renderer.command_list->DrawIndexedInstanced(36, 1, 0, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    renderer.command_list->ResourceBarrier(1, &barrier);

    ThrowIfFailed(renderer.command_list->Close());

    ID3D12CommandList* ppCommandLists[] = { renderer.command_list.Get() };
    renderer.command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(renderer.swap_chain->Present(1, 0));

    WaitForPreviousFrame();
}

void onDestroy() {
    WaitForPreviousFrame();
    CloseHandle(g_fenceEvent);
}

void WaitForPreviousFrame() {
    const UINT64 fence = g_fenceValue;
    ThrowIfFailed(renderer.command_queue->Signal(renderer.fence.Get(), fence));
    g_fenceValue++;

    if (renderer.fence->GetCompletedValue() < fence) {
        ThrowIfFailed(renderer.fence->SetEventOnCompletion(fence, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    FRAME_INDEX = renderer.swap_chain->GetCurrentBackBufferIndex();
}
