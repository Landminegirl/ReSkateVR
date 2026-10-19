#include "vr_pointer_gpu.h"
namespace dingosdk::vr::detail {
bool PointerPainter::wait(UINT64 value, DWORD timeout) noexcept {
    if (!value || !fence_) return true;
    const auto complete = fence_->GetCompletedValue();
    if (complete == UINT64_MAX) return false;
    if (complete >= value) return true;
    return SUCCEEDED(fence_->SetEventOnCompletion(value, event_)) && WaitForSingleObject(event_, timeout) == WAIT_OBJECT_0 &&
        fence_->GetCompletedValue() >= value;
}
bool PointerPainter::wait_idle(DWORD timeout) noexcept { return wait(next_, timeout); }
void PointerPainter::destroy() noexcept {
    (void)wait_idle(2000);
    list_.Reset(); rtvs_.Reset(); fence_.Reset(); device_.Reset();
    for (auto& allocator : allocators_) allocator.Reset();
    if (event_) CloseHandle(event_);
    event_ = nullptr; fences_ = {}; slot_ = 0; next_ = 0;
}
bool PointerPainter::create(ID3D12Device* device, std::string& error) {
    if (device_) return true;
    D3D12_DESCRIPTOR_HEAP_DESC heap{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, slots};
    bool ok = SUCCEEDED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvs_)));
    for (auto& allocator : allocators_)
        ok = ok && SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    ok = ok && SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(), nullptr, IID_PPV_ARGS(&list_)));
    if (ok) ok = SUCCEEDED(list_->Close());
    ok = ok && SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!ok || !event_) { error = "Creating the controller pointer graphics failed."; destroy(); return false; }
    stride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device_ = device;
    return true;
}
bool PointerPainter::paint(ID3D12CommandQueue* queue, ID3D12Resource* target, DXGI_FORMAT format, bool cursor, bool pressed, std::string& error) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))) || !create(device.Get(), error)) return false;
    if (!wait(fences_[slot_], 1000) || FAILED(allocators_[slot_]->Reset()) || FAILED(list_->Reset(allocators_[slot_].Get(), nullptr))) {
        error = "The GPU did not finish an earlier pointer draw."; return false;
    }
    auto rtv = rtvs_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(slot_) * stride_;
    D3D12_RENDER_TARGET_VIEW_DESC view{};
    view.Format = format;
    view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(target, &view, rtv);
    const float clear[]{0,0,0,0}, blue[]{.06f,.55f,1,1}, selected[]{1,.7f,.08f,1};
    const auto* color = pressed ? selected : blue;
    if (cursor) {
        list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
        const D3D12_RECT cross[]{{2,14,30,18},{14,2,18,30}};
        list_->ClearRenderTargetView(rtv, color, 2, cross);
    } else list_->ClearRenderTargetView(rtv, color, 0, nullptr);
    if (FAILED(list_->Close())) { error = "Closing the pointer draw failed."; return false; }
    ID3D12CommandList* lists[]{list_.Get()};
    queue->ExecuteCommandLists(1, lists);
    const auto signal = ++next_;
    if (FAILED(queue->Signal(fence_.Get(), signal))) { error = "Signalling the pointer fence failed."; return false; }
    fences_[slot_] = signal;
    slot_ = (slot_ + 1) % slots;
    return true;
}
}
