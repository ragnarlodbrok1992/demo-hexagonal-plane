
// Configuration variables - might go even before we include stuff if this stuff ends up in different files
constexpr int DISPLAY_FACTOR = 100;
constexpr int DISPLAY_WIDTH = 16 * DISPLAY_FACTOR;
constexpr int DISPLAY_HEIGHT = 9 * DISPLAY_FACTOR;

// Top-level defines
#include "graphics/defines.cpp"

#include "win32_renderer.cpp"
#include "win_utils.cpp"
#include "camera.cpp"
#include "input.cpp"
#include "entities/cube.cpp"
#include "shaders/win32_default_shaders.cpp"
#include "render_pipeline/on_init.cpp"
#include "render_pipeline/on_init_compile_shaders.cpp"

#include <Windows.h>
#include <wrl.h>
#include <cstdlib>
#include <ctime>

// Global variables
HWND g_hwnd = NULL;

// Pack this together? This is used for objects that are being renderer
// Entities on the screen.
Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;

D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
D3D12_INDEX_BUFFER_VIEW indexBufferView;
UINT8* constantBufferView;  // There is something called D3D12_CONSTANT_BUFFER_VIEW_DESC in d3d12.h


typedef struct {
    DirectX::XMMATRIX world;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
} MVPMatrix;

// TODO(ragnar): Rename it to something better
MVPMatrix constantBufferData;  // MVP matrices go here - stuff that lands in shader
MVPMatrix cameraData;

// Forward declarations - maybe remove them?
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
        CW_USEDEFAULT, CW_USEDEFAULT,
        DISPLAY_WIDTH, DISPLAY_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (g_hwnd == NULL) {
        return 0;
    }

    // TODO(ragnar): Remove exception
    // TODO(ragnar): Split this calls - we don't know what fails
    try {
        onInit(renderer, g_hwnd); // Some stuff get's passed, because for other renderers we need to have window handler in main module
        onInitCompileShaders(renderer, shaders, win32_shader);
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

void prepareCamera () {
    // Renderer initialized, below stuff is engine related
    
    cameraData.world = DirectX::XMMatrixIdentity();

    // Maybe we can write something about DirectX3D space coordinates?
    // It's right hand - pointed straight to our face. Thumb is y, second finger is x, first finger is z.
    // -z to our face.
    // +y up.
    // Rest is history
    DirectX::XMVECTOR eye = DirectX::XMVectorSet(2.0f, 2.0f, -2.0f, 0.0f);
    DirectX::XMVECTOR at  = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up  = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    cameraData.view = DirectX::XMMatrixLookAtLH(eye, at, up);

    // Perspective projection provided straight from microsoft vaults.
    cameraData.projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4,
        (float)DISPLAY_WIDTH / (float)DISPLAY_HEIGHT,
        0.1f,
        100.0f);
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
    cameraData.world = rotationMatrix;

    // Transpose the matrices to be column-major for the shader.
    constantBufferData.world = DirectX::XMMatrixTranspose(cameraData.world);
    constantBufferData.view = DirectX::XMMatrixTranspose(cameraData.view);
    constantBufferData.projection = DirectX::XMMatrixTranspose(cameraData.projection);
    
    memcpy(constantBufferView, &constantBufferData, sizeof(constantBufferData));
}

void onRender() {
    // Begin stuff - always has to be done independent of what is being rendered?
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, DISPLAY_WIDTH, DISPLAY_HEIGHT, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT scissorRect = { 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT };
    D3D12_RESOURCE_BARRIER barrier = {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    ID3D12CommandList* ppCommandLists[] = { renderer.command_list.Get() };

    ThrowIfFailed(renderer.command_allocator->Reset());
    ThrowIfFailed(renderer.command_list->Reset(renderer.command_allocator.Get(), renderer.pipeline_state.Get()));
    renderer.command_list->SetGraphicsRootSignature(renderer.root_signature.Get());
    renderer.command_list->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
    renderer.command_list->RSSetViewports(1, &viewport);
    renderer.command_list->RSSetScissorRects(1, &scissorRect);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderer.render_targets[FRAME_INDEX].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderer.command_list->ResourceBarrier(1, &barrier);
    rtvHandle.ptr += FRAME_INDEX * RTV_DESCRIPTOR_SIZE;
    renderer.command_list->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    
    // Render stuff goes here - vertexBufferView and indexBufferView
    renderer.command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    renderer.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderer.command_list->IASetVertexBuffers(0, 1, &vertexBufferView);
    renderer.command_list->IASetIndexBuffer(&indexBufferView);
    renderer.command_list->DrawIndexedInstanced(36, 1, 0, 0, 0);

    // End stuff - always has to be done independent of what is being rendered?
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    renderer.command_list->ResourceBarrier(1, &barrier);
    ThrowIfFailed(renderer.command_list->Close());
    renderer.command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(renderer.swap_chain->Present(1, 0));
    WaitForPreviousFrame();
}

void onDestroy() {
    WaitForPreviousFrame();
    CloseHandle(g_fenceEvent);
}

// TODO(moliwa): This should go to onRender pipline and stay there forever and ever
// This function is only called in onRender
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
