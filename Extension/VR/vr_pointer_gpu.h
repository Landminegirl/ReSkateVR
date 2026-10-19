#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <string>

namespace dingosdk::vr::detail {
// Tiny compositor textures: a solid beam and an alpha crosshair. No scene pass.
class PointerPainter {
public:
    bool paint(ID3D12CommandQueue* queue, ID3D12Resource* target, DXGI_FORMAT format, bool cursor, bool pressed, std::string& error);
    bool wait_idle(DWORD timeout) noexcept;
    void destroy() noexcept;
private:
    static constexpr UINT slots = 6;
    bool create(ID3D12Device* device, std::string& error);
    bool wait(UINT64 value, DWORD timeout) noexcept;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvs_;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, slots> allocators_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::array<UINT64, slots> fences_{};
    HANDLE event_ = nullptr;
    UINT stride_ = 0, slot_ = 0;
    UINT64 next_ = 0;
};
}
