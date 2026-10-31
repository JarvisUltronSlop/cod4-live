#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <string>
namespace live::hosting {
inline bool NewToken(char (&token)[33]) {
    unsigned char bytes[16];
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    const char* hex = "0123456789abcdef";
    for (int i=0;i<16;++i) { token[2*i]=hex[bytes[i]>>4]; token[2*i+1]=hex[bytes[i]&15]; }
    token[32]=0; return true;
}
inline std::wstring Quote(std::wstring_view value) {
    std::wstring out=L"\""; size_t slashes=0;
    for (wchar_t c:value) {
        if(c==L'\\') { ++slashes; continue; }
        out.append(c==L'"' ? slashes*2+1 : slashes,L'\\'); slashes=0; out+=c;
    }
    out.append(slashes*2,L'\\'); out+=L'"'; return out;
}
}
