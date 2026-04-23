#pragma once
#include <cstdint>
#include <map>
#include <string>

// Minimal Arduino-String shim for native builds.
class String {
    std::string s;
public:
    String() {}
    String(const char* p) : s(p ? p : "") {}
    const char* c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    bool operator==(const char* r) const { return s == r; }
};

class Preferences {
public:
    std::map<std::string, std::string> strs;
    std::map<std::string, uint32_t>    u32s;

    bool begin(const char*, bool = false) { return true; }
    void end() {}
    size_t putString(const char* k, const char* v) { strs[k] = v; return 1; }
    size_t putUInt(const char* k, uint32_t v)      { u32s[k] = v; return 1; }
    size_t putUChar(const char* k, uint8_t v)      { u32s[k] = v; return 1; }
    size_t putUShort(const char* k, uint16_t v)    { u32s[k] = v; return 1; }
    String getString(const char* k, const char* d = "") {
        auto it = strs.find(k); return String(it == strs.end() ? d : it->second.c_str());
    }
    uint32_t getUInt(const char* k, uint32_t d = 0)    { auto it = u32s.find(k); return it == u32s.end() ? d : it->second; }
    uint16_t getUShort(const char* k, uint16_t d = 0)  { auto it = u32s.find(k); return it == u32s.end() ? d : (uint16_t)it->second; }
    uint8_t  getUChar(const char* k, uint8_t d = 0)    { auto it = u32s.find(k); return it == u32s.end() ? d : (uint8_t)it->second; }
};
