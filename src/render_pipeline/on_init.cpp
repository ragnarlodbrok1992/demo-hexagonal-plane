#ifndef _H_RENDER_PIPELINE_ON_INIT
#define _H_RENDER_PIPELINE_ON_INIT

// constexpr int SOME_VALUE = 2137 * DISPLAY_FACTOR; // Some stuff might need to be defined even before we include it
// Before I lost track of resources - renderer is defined in win32_renderer as static global structure
// In this case this is actually not function at all but a procedure that manipulates global state to prepare the renderer
static inline void onInit(win32_Renderer& renderer, HWND& g_hwnd) {
  // Define debug layer here - initialization
  // There is another "debug" stuff being initialized that might be important in the future and it's in compiling shaders
#if defined(DEBUG_DIRECTX)
      {
        #pragma message("DEBUG_DIRECTX defined - setting up debug layer for DirectX3D 12.")
          Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
          if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
              debugController->EnableDebugLayer();
          }
      }
#endif

      // Idea here is to group what we need to initialize renderer and what needs to be done.
      // Prepare required resources to move on
      Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
      D3D12_COMMAND_QUEUE_DESC queueDesc = {};
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
      DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
      D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
      D3D12_ROOT_PARAMETER rootParameters[1];
      D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
      Microsoft::WRL::ComPtr<ID3DBlob> signature = {}; // Zero initialization instead of only declaring
      Microsoft::WRL::ComPtr<ID3DBlob> error = {}; // Zero initialization instead of only declaring

      // Proceed here
      // 1. Creating device
      ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
      ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&renderer.device)));

      // 2. Creating command queue
      queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
      queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
      ThrowIfFailed(renderer.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer.command_queue)));

      // 3. Creating swapchain
      swapChainDesc.BufferCount = BUFFER_COUNT;
      swapChainDesc.Width = DISPLAY_WIDTH;
      swapChainDesc.Height = DISPLAY_HEIGHT;
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

      // 4. Creating descriptor heap 
      rtvHeapDesc.NumDescriptors = BUFFER_COUNT;
      rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      ThrowIfFailed(renderer.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&renderer.rtv_heap)));
      RTV_DESCRIPTOR_SIZE = renderer.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      
      // 5. Creating render targets with iterating over because of BUFFER COUNTS
      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
      for (UINT n = 0; n < BUFFER_COUNT; n++) {
          ThrowIfFailed(renderer.swap_chain->GetBuffer(n, IID_PPV_ARGS(&renderer.render_targets[n])));
          renderer.device->CreateRenderTargetView(renderer.render_targets[n].Get(), nullptr, rtvHandle);
          rtvHandle.ptr += RTV_DESCRIPTOR_SIZE;
      }

      // 6. Creating command allocator
      ThrowIfFailed(renderer.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderer.command_allocator)));

      // 7. Creating root signature
      rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
      rootParameters[0].Descriptor.ShaderRegister = 0;
      rootParameters[0].Descriptor.RegisterSpace = 0;
      rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
      rootSignatureDesc.NumParameters = _countof(rootParameters);
      rootSignatureDesc.pParameters = rootParameters;
      rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
      ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
      ThrowIfFailed(renderer.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&renderer.root_signature)));
}

#endif /* _H_RENDER_PIPELINE_ON_INIT */
