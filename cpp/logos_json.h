#pragma once
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

// Semantic aliases for nlohmann::json used in universal module impl classes.
// The code generator recognizes these names and emits QVariantMap / QVariantList
// conversions in the Qt glue layer, so impl classes remain Qt-free.
using LogosMap  = nlohmann::json;
using LogosList = nlohmann::json;

namespace logos {

// The canonical tagged form for binary payloads on the wire:
//
//     {"_bytes": "<base64url, unpadded>"}
//
// It is what logos-protocol emits and expects (logos_json_convert.cpp,
// implementations/plain/json_mapping.cpp), and it is lossless for arbitrary
// bytes — including embedded NULs, which a plain JSON string would not survive.
// The Qt side reaches this form through QByteArray::toBase64/fromBase64 with
// Base64UrlEncoding | OmitTrailingEquals; these are the Qt-free equivalents,
// used by the generated `lp` wrappers and by universal (Qt-free) module code.

inline std::string b64UrlEncode(const std::vector<uint8_t>& bytes)
{
    static const char* alpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    size_t i = 0;
    while (i + 3 <= bytes.size()) {
        uint32_t n = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i + 1]) << 8)
                   | uint32_t(bytes[i + 2]);
        out += alpha[(n >> 18) & 0x3f]; out += alpha[(n >> 12) & 0x3f];
        out += alpha[(n >> 6) & 0x3f];  out += alpha[n & 0x3f];
        i += 3;
    }
    if (i < bytes.size()) {
        uint32_t n = uint32_t(bytes[i]) << 16;
        if (i + 1 < bytes.size()) n |= uint32_t(bytes[i + 1]) << 8;
        out += alpha[(n >> 18) & 0x3f]; out += alpha[(n >> 12) & 0x3f];
        if (i + 1 < bytes.size()) out += alpha[(n >> 6) & 0x3f];
    }
    return out;
}

inline std::vector<uint8_t> b64UrlDecode(const std::string& in)
{
    auto idx = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9') return ch - '0' + 52;
        if (ch == '-') return 62;
        if (ch == '_') return 63;
        return -1;  // skips '=' padding and any stray character
    };
    std::vector<uint8_t> out;
    uint32_t buf = 0;
    int bits = 0;
    for (char ch : in) {
        const int v = idx(ch);
        if (v < 0) continue;
        buf = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xff));
        }
    }
    return out;
}

// Bytes -> the tagged JSON object.
inline nlohmann::json bytesToJson(const std::vector<uint8_t>& bytes)
{
    return nlohmann::json{{"_bytes", b64UrlEncode(bytes)}};
}

// The tagged JSON object -> bytes. Lenient, like the rest of the `lp` decode
// path: anything that is not a well-formed tagged-bytes object yields empty.
inline std::vector<uint8_t> jsonToBytes(const nlohmann::json& j)
{
    if (!j.is_object() || !j.contains("_bytes") || !j["_bytes"].is_string())
        return {};
    return b64UrlDecode(j["_bytes"].get<std::string>());
}

} // namespace logos
