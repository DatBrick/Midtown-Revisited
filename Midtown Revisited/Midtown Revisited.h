#pragma once

void Initialize();

template <typename... T>
inline void DebugPrint(const char* format, T&&... args)
{
    char buffer[2048];
    std::snprintf(buffer, std::size(buffer), format, std::forward<T>(args)...);

    OutputDebugStringA(std::string("[Midtown Revisited] | ").append(buffer).append("\n").c_str());
}

