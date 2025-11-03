/*
MIT License

Copyright (c) 2025 Pawe³ C. (PaweX3)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 * ============================================================================
 *  FILE:       pid_convert.h
 *  AUTHOR:     Pawe³ C. (PaweX3)
 *  LICENSE:    MIT
 *
 *  BRIEF:      Plugin header for Dragon UnPACKer 5 (.PID format)
 *
 *  DETAILS:
 *      Contains plugin metadata (name, version, author),
 *      DUCI structures (ShortString, ConvertList, ConvertInfoRec),
 *      the .PID file header structure, default 256-color palette,
 *      PNGMode enum, crc32_png function, and helpers for
 *      ShortString ? std::string conversion.
 *      All structures are packed and ready for export.
 * ============================================================================
 */

#pragma once
#ifndef PID_CONVERTER_H
#define PID_CONVERTER_H

#include <cstddef>
#include <string>
#include <cstring>


// Debug-only macro that formats a message and sends it to the Visual Studio debugger output (no-op in release builds).
#ifdef _DEBUG
#define DBG_MSG(fmt, ...) do { \
    char __buf[512]; \
    _snprintf_s(__buf, sizeof(__buf), _TRUNCATE, fmt, __VA_ARGS__); \
    OutputDebugStringA(__buf); \
} while(0)
#else
#define DBG_MSG(...) ((void)0)
#endif


// =======================
// Plugin metadata constants
// =======================
#define PLUGIN_NAME     "Gruntz (1999) .PID converter"
#define PLUGIN_VERSION  "0.82"
#define PLUGIN_AUTHOR   "Pawe³ C. (PaweX3)"
#define PLUGIN_COMMENT  "Converts .PID graphic filez to BMP/TGA/PNG"


// About Message:
constexpr std::string_view MSG_about =
PLUGIN_NAME " plugin v" PLUGIN_VERSION "\n"
"Created by " PLUGIN_AUTHOR ".\n"
"Converts Gruntz .PID filez to BMP, TGA and PNG with alpha support.\n"
"Designed for Dragon UnPACKer 5, DUCI v4\n";


// =======================
// Type declarations
// =======================
typedef unsigned char DBOOL; // Delphi Boolean (1 byte)

// =======================

// Color structure (RGBA)
struct Color
{
    unsigned char r, g, b, a;
};

// Enumeration for PNG output mode
enum class PNGMode : unsigned short
{
    PNG_8 = 8,   // 8bpp paletted
    PNG_24 = 24, // 24bpp true-color
    PNG_32 = 32  // 32bpp RGBA
};


// =======================
// DUCI-compatible structures
// =======================
#pragma pack(push, 1) // Packed for Delphi ABI compatibility

// Delphi ShortString - fixed size 256 bytes
struct ShortString
{
    unsigned char len;
    char data[255];
};

// Convert list element (DUCI-compatible)
struct ConvertListElem
{
    ShortString Display;
    ShortString Ext;
    ShortString ID;
};

// Convert list (DUCI-compatible)
struct ConvertList
{
    unsigned char NumFormats;       // number of formats
    ConvertListElem List[255];      // entries 0..NumFormats-1
};

// Plugin information (DUCI-compatible)
struct ConvertInfoRec
{
    ShortString Name;
    ShortString Version;
    ShortString Author;
    ShortString Comment;
    int VerID;
};

// PID file header (specific to this plugin)
struct PIDHeader
{
    int ID;
    int Flags;
    int Width;
    int Height;
    int U[4];
};

#pragma pack(pop)


// =======================
// Global variables
// =====================//
static PNGMode g_default_PNG_Mode = PNGMode::PNG_8; // default 8 bpp

// Default palette (256 colors)
static const Color defaultPalette[256] = {
    {0, 0, 0, 255}, {128, 0, 0, 255}, {0, 128, 0, 255}, {128, 128, 0, 255}, {0, 0, 128, 255}, {128, 0, 128, 255}, {0, 128, 128, 255}, {192, 192, 192, 255},
    {192, 220, 192, 255}, {166, 202, 240, 255}, {42, 63, 170, 255}, {42, 63, 255, 255}, {42, 95, 0, 255}, {42, 95, 85, 255}, {42, 95, 170, 255}, {42, 95, 255, 255},
    {42, 127, 0, 255}, {42, 127, 85, 255}, {42, 127, 170, 255}, {42, 127, 255, 255}, {42, 159, 0, 255}, {42, 159, 85, 255}, {42, 159, 170, 255}, {42, 159, 255, 255},
    {42, 191, 0, 255}, {42, 191, 85, 255}, {42, 191, 170, 255}, {42, 191, 255, 255}, {42, 223, 0, 255}, {42, 223, 85, 255}, {42, 223, 170, 255}, {42, 223, 255, 255},
    {42, 255, 0, 255}, {42, 255, 85, 255}, {42, 255, 170, 255}, {42, 255, 255, 255}, {85, 0, 0, 255}, {85, 0, 85, 255}, {85, 0, 170, 255}, {85, 0, 255, 255},
    {85, 31, 0, 255}, {85, 31, 85, 255}, {85, 31, 170, 255}, {85, 31, 255, 255}, {85, 63, 0, 255}, {85, 63, 85, 255}, {85, 63, 170, 255}, {85, 63, 255, 255},
    {85, 95, 0, 255}, {85, 95, 85, 255}, {85, 95, 170, 255}, {85, 95, 255, 255}, {85, 127, 0, 255}, {85, 127, 85, 255}, {85, 127, 170, 255}, {85, 127, 255, 255},
    {85, 159, 0, 255}, {85, 159, 85, 255}, {85, 159, 170, 255}, {85, 159, 255, 255}, {85, 191, 0, 255}, {85, 191, 85, 255}, {85, 191, 170, 255}, {85, 191, 255, 255},
    {85, 223, 0, 255}, {85, 223, 85, 255}, {85, 223, 170, 255}, {85, 223, 255, 255}, {85, 255, 0, 255}, {85, 255, 85, 255}, {85, 255, 170, 255}, {85, 255, 255, 255},
    {127, 0, 0, 255}, {127, 0, 85, 255}, {127, 0, 170, 255}, {127, 0, 255, 255}, {127, 31, 0, 255}, {127, 31, 85, 255}, {127, 31, 170, 255}, {127, 31, 255, 255},
    {127, 63, 0, 255}, {127, 63, 85, 255}, {127, 63, 170, 255}, {127, 63, 255, 255}, {127, 95, 0, 255}, {127, 95, 85, 255}, {127, 95, 170, 255}, {127, 95, 255, 255},
    {127, 127, 0, 255}, {127, 127, 85, 255}, {127, 127, 170, 255}, {127, 127, 255, 255}, {127, 159, 0, 255}, {127, 159, 85, 255}, {127, 159, 170, 255}, {127, 159, 255, 255},
    {127, 191, 0, 255}, {127, 191, 85, 255}, {127, 191, 170, 255}, {127, 191, 255, 255}, {127, 223, 0, 255}, {127, 223, 85, 255}, {127, 223, 170, 255}, {127, 223, 255, 255},
    {127, 255, 0, 255}, {127, 255, 85, 255}, {127, 255, 170, 255}, {127, 255, 255, 255}, {170, 0, 0, 255}, {170, 0, 85, 255}, {170, 0, 170, 255}, {170, 0, 255, 255},
    {170, 31, 0, 255}, {170, 31, 85, 255}, {170, 31, 170, 255}, {170, 31, 255, 255}, {170, 63, 0, 255}, {170, 63, 85, 255}, {170, 63, 170, 255}, {170, 63, 255, 255},
    {170, 95, 0, 255}, {170, 95, 85, 255}, {170, 95, 170, 255}, {170, 95, 255, 255}, {170, 127, 0, 255}, {170, 127, 85, 255}, {170, 127, 170, 255}, {170, 127, 255, 255},
    {170, 159, 0, 255}, {170, 159, 85, 255}, {170, 159, 170, 255}, {170, 159, 255, 255}, {170, 191, 0, 255}, {170, 191, 85, 255}, {170, 191, 170, 255}, {170, 191, 255, 255},
    {170, 223, 0, 255}, {170, 223, 85, 255}, {170, 223, 170, 255}, {170, 223, 255, 255}, {170, 255, 0, 255}, {170, 255, 85, 255}, {170, 255, 170, 255}, {170, 255, 255, 255},
    {212, 0, 0, 255}, {212, 0, 85, 255}, {212, 0, 170, 255}, {212, 0, 255, 255}, {212, 31, 0, 255}, {212, 31, 85, 255}, {212, 31, 170, 255}, {212, 31, 255, 255},
    {212, 63, 0, 255}, {212, 63, 85, 255}, {212, 63, 170, 255}, {212, 63, 255, 255}, {212, 95, 0, 255}, {212, 95, 85, 255}, {212, 95, 170, 255}, {212, 95, 255, 255},
    {212, 127, 0, 255}, {212, 127, 85, 255}, {212, 127, 170, 255}, {212, 127, 255, 255}, {212, 159, 0, 255}, {212, 159, 85, 255}, {212, 159, 170, 255}, {212, 159, 255, 255},
    {212, 191, 0, 255}, {212, 191, 85, 255}, {212, 191, 170, 255}, {212, 191, 255, 255}, {212, 223, 0, 255}, {212, 223, 85, 255}, {212, 223, 170, 255}, {212, 223, 255, 255},
    {212, 255, 0, 255}, {212, 255, 85, 255}, {212, 255, 170, 255}, {212, 255, 255, 255}, {255, 0, 85, 255}, {255, 0, 170, 255}, {255, 31, 0, 255}, {255, 31, 85, 255},
    {255, 31, 170, 255}, {255, 31, 255, 255}, {255, 63, 0, 255}, {255, 63, 85, 255}, {255, 63, 170, 255}, {255, 63, 255, 255}, {255, 95, 0, 255}, {255, 95, 85, 255},
    {255, 95, 170, 255}, {255, 95, 255, 255}, {255, 127, 0, 255}, {255, 127, 85, 255}, {255, 127, 170, 255}, {255, 127, 255, 255}, {255, 159, 0, 255}, {255, 159, 85, 255},
    {255, 159, 170, 255}, {255, 159, 255, 255}, {255, 191, 0, 255}, {255, 191, 85, 255}, {255, 191, 170, 255}, {255, 191, 255, 255}, {255, 223, 0, 255}, {255, 223, 85, 255},
    {255, 223, 170, 255}, {255, 223, 255, 255}, {255, 255, 85, 255}, {255, 255, 170, 255}, {204, 204, 255, 255}, {255, 204, 255, 255}, {51, 255, 255, 255}, {102, 255, 255, 255},
    {153, 255, 255, 255}, {204, 255, 255, 255}, {0, 127, 0, 255}, {0, 127, 85, 255}, {0, 127, 170, 255}, {0, 127, 255, 255}, {0, 159, 0, 255}, {0, 159, 85, 255},
    {0, 159, 170, 255}, {0, 159, 255, 255}, {0, 191, 0, 255}, {0, 191, 85, 255}, {0, 191, 170, 255}, {0, 191, 255, 255}, {0, 223, 0, 255}, {0, 223, 85, 255},
    {0, 223, 170, 255}, {0, 223, 255, 255}, {0, 255, 85, 255}, {0, 255, 170, 255}, {42, 0, 0, 255}, {42, 0, 85, 255}, {42, 0, 170, 255}, {42, 0, 255, 255},
    {42, 31, 0, 255}, {42, 31, 85, 255}, {42, 31, 170, 255}, {42, 31, 255, 255}, {42, 63, 0, 255}, {42, 63, 85, 255}, {255, 251, 240, 255}, {160, 160, 164, 255},
    {128, 128, 128, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}, {255, 255, 0, 255}, {0, 0, 255, 255}, {255, 0, 255, 255}, {0, 255, 255, 255}, {255, 255, 255, 255}
};


// =======================
// Inline functions
// =======================

// Inline CRC32 function for PNG chunks
static inline unsigned int crc32_png(const unsigned char *data, size_t len, unsigned int crc = 0)
{
    static const unsigned int crc_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a56, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
    if (len == 0) return ~crc;
    crc = ~crc;
    for (size_t i = 0; i < len; ++i)
    {
        crc = crc_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return ~crc;
}

// Converts a ShortString into a std::string using its length field.
inline std::string ShortStringToString(const ShortString &ss) noexcept
{
    size_t len = ss.len > 255 ? 255 : ss.len;
    return std::string(ss.data, ss.data + len);
}

// Small wrapper: ShortString* -> std::string (nullptr-safe)
inline std::string ShortStringPtrToString(const ShortString *ss)
{
    return ss ? ShortStringToString(*ss) : std::string();
}

// Writes a C?string into a ShortString, truncating to 255 bytes and updating the length.
inline void WriteShortString(ShortString &dst, const char *src) noexcept
{
    if (!src) { dst.len = 0; return; }
    size_t srcLen = std::strlen(src);
    unsigned char len = static_cast<unsigned char>(srcLen > 255 ? 255 : srcLen);
    dst.len = len;
    if (len) std::memcpy(dst.data, src, len);
    if (len < 255) dst.data[len] = '\0';
}

// Copies a ShortString into a destination C?string buffer with proper null?termination.
inline void ShortStringToCStr(const ShortString *src, char *dest, size_t destSize) noexcept
{
    if (destSize == 0) return;
    if (!src || src->len == 0) { dest[0] = '\0'; return; }
    int len = static_cast<int>(src->len);
    if (len >= static_cast<int>(destSize)) len = static_cast<int>(destSize) - 1;
    std::memcpy(dest, src->data, len);
    dest[len] = '\0';
}

#endif // PID_CONVERTER_H