#pragma once
#include <cstring>
#include <string_view>
namespace dingosdk::vr::chat_text {
// Whole-key insertion only: never split a UTF-8 code point at the chat byte limit.
inline bool append(char* text, std::size_t capacity, std::string_view key) noexcept {
    if (!text || !capacity) return false;
    const auto length = std::strlen(text);
    if (length >= capacity || key.size() >= capacity - length) return false;
    std::memcpy(text + length, key.data(), key.size());
    text[length + key.size()] = 0;
    return true;
}
inline void backspace(char* text) noexcept {
    if (!text) return;
    auto length = std::strlen(text);
    if (!length) return;
    --length;
    while (length && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80) --length;
    text[length] = 0;
}
inline bool sendable(std::string_view text) noexcept {
    return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
}
}
