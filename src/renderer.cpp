#ifndef _H_RENDERER
#define _H_RENDERER

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

constexpr UINT BUFFER_COUNT = 2;

UINT RTV_DESCRIPTOR_SIZE;
UINT FRAME_INDEX;


struct win32_Renderer {
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
  Microsoft::WRL::ComPtr<ID3D12Resource> render_targets[BUFFER_COUNT];
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
};

struct win32_Shaders {
  Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
};


#endif /* _H_RENDERER */

