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
 *  FILE:       pid_convert.cpp
 *  AUTHOR:     Pawe³ C. (PaweX3)
 *  LICENSE:    MIT
 *
 *  BRIEF:      DUCI v3/v4 plugin for Dragon UnPACKer 5 (.PID format)
 *
 *  DETAILS:
 *      Main implementation of the DUCI plugin for the .PID format (Gruntz 1999).
 *      Exports 7 DUCI functions (DUCIVersion, VersionInfo2, IsFileCompatible,
 *      GetFileConvert, ConvertStream, InitPlugin, ConfigBox, AboutBox).
 *      Handles RLE decompression, palette (default or external), flags
 *      (transparency, mirror, invert), and export to:
 *          - BMP (24bpp BGR)
 *          - TGA (8bpp paletted)
 *          - PNG (8/24/32bpp with zlib)
 *
 *      Uses DelphiTStreamWrapper for safe I/O with SEH protection,
 *      includes debug logging (DBG_MSG), a configuration dialog (PNG mode),
 *      and full plugin metadata.
 * ============================================================================
 */

#include "delphiTStreamWrapper.h" // MUST BE BEFORE windows.h!
#include <windows.h> // AT THE BEGINNING (MessageBoxA, HWND, etc.)
#include "resource.h"
#include <vector>
#include <cstring>
#include <zlib.h>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include "pid_convert.h"



// ===================================================================
// DU callbacks (progress, translations, host handle etc.)
// ===================================================================
typedef void(__stdcall *TPercentCallback)(BYTE p);
typedef const char *(__stdcall *TLanguageCallback)(const char *lngid);
typedef int(__stdcall *TMsgBoxCallback)(const char *title, const char *msg, unsigned int flags);

// ===================================================================
// Global variables (set by InitPlugin)
// ===================================================================
TPercentCallback Percent = nullptr;
TLanguageCallback DLNGStr = nullptr;
HWND AHandle = nullptr;
void *AOwner = nullptr;
char CurPath[256] = { 0 };

// ===================================================================
// Plugin metadata constants (from pid_convert.h)
// ===================================================================
static const ConvertInfoRec pluginInfo = [] {
    ConvertInfoRec tmp{};
    WriteShortString(tmp.Name, PLUGIN_NAME);
    WriteShortString(tmp.Version, PLUGIN_VERSION);
    WriteShortString(tmp.Author, PLUGIN_AUTHOR);
    WriteShortString(tmp.Comment, PLUGIN_COMMENT);
    tmp.VerID = 0x00010000;
    return tmp;
}();

// ===================================================================
// Helper: write PNG chunk (internal linkage)
// ===================================================================
static void write_PNG_Chunk(std::vector<char> &buffer, const char *type, const unsigned char *data, unsigned int length)
{
    // Writes length (big-endian), type, data and CRC into the buffer
    unsigned int len = _byteswap_ulong(length);
    buffer.insert(buffer.end(), (char *)&len, (char *)&len + 4);
    buffer.insert(buffer.end(), type, type + 4);
    if (data && length > 0)
    {
        buffer.insert(buffer.end(), data, data + length);
    }
    unsigned int crc = crc32_png(reinterpret_cast<const unsigned char *>(type), 4);
    if (data && length > 0)
    {
        crc = crc32_png(data, length, crc);
    }
    crc = _byteswap_ulong(crc);
    buffer.insert(buffer.end(), (char *)&crc, (char *)&crc + 4);
}

// ===================================================================
// Save as BMP 24bpp (true-color, BGR)
// - Writes fields explicitly in little-endian to avoid padding issues.
// - Uses palette and supports transparency.
// ===================================================================
static int SaveToBMP(DelphiTStreamWrapper &dst,
              const std::vector<unsigned char> &pixels,
              int width, int height,
              const Color *palette_src,
              bool useTransparency)
{
    // Copy palette and optionally zero index 0 (transparent -> black)
    Color palette[256];
    std::memcpy(palette, palette_src, sizeof(palette));
    if (useTransparency)
    {
        palette[0].r = 0;
        palette[0].g = 0;
        palette[0].b = 0;
    }

    // Image parameters
    const int bpp = 24;                            // we write 24bpp BGR (true-color)
    const int rowSize = ((width * 3 + 3) & ~3);    // align to 4 bytes
    const int imageSize = rowSize * height;
    const uint32_t infoSize = 40;                  // BITMAPINFOHEADER size
    const uint32_t paletteBytes = 0;               // no palette for 24bpp in BMP DIB
    const uint32_t dataOffset = 14 + infoSize + static_cast<uint32_t>(paletteBytes);
    const uint32_t fileSize = dataOffset + static_cast<uint32_t>(imageSize);

    // Helper lambdas to write LE 16/32-bit
    auto write_u16 = [&](uint16_t v) -> bool {
        unsigned char b[2] = {
            static_cast<unsigned char>(v & 0xFF),
            static_cast<unsigned char>((v >> 8) & 0xFF)
        };
        return dst.write(b, 2) == 2;
    };
    auto write_u32 = [&](uint32_t v) -> bool {
        unsigned char b[4] = {
            static_cast<unsigned char>(v & 0xFF),
            static_cast<unsigned char>((v >> 8) & 0xFF),
            static_cast<unsigned char>((v >> 16) & 0xFF),
            static_cast<unsigned char>((v >> 24) & 0xFF)
        };
        return dst.write(b, 4) == 4;
    };
    auto write_s32 = [&](int32_t v) -> bool {
        return write_u32(static_cast<uint32_t>(v));
    };

    // --- BITMAPFILEHEADER (14 bytes) ---
    // signature "BM"
    unsigned char sig[2] = { 'B', 'M' };
    if (dst.write(sig, 2) != 2) { DBG_MSG("SaveToBMP: failed writing signature\n"); return 1; }
    // fileSize (4)
    if (!write_u32(fileSize)) { DBG_MSG("SaveToBMP: failed writing fileSize\n"); return 1; }
    // reserved1 (2) = 0
    if (!write_u16(0)) { DBG_MSG("SaveToBMP: failed writing reserved1\n"); return 1; }
    // reserved2 (2) = 0
    if (!write_u16(0)) { DBG_MSG("SaveToBMP: failed writing reserved2\n"); return 1; }
    // dataOffset (4)
    if (!write_u32(dataOffset)) { DBG_MSG("SaveToBMP: failed writing dataOffset\n"); return 1; }

    // --- BITMAPINFOHEADER (40 bytes) ---
    if (!write_u32(infoSize)) { DBG_MSG("SaveToBMP: failed writing infoSize\n"); return 1; }
    if (!write_s32(static_cast<int32_t>(width))) { DBG_MSG("SaveToBMP: failed writing width\n"); return 1; }
    // height positive => bottom-up
    if (!write_s32(static_cast<int32_t>(height))) { DBG_MSG("SaveToBMP: failed writing height\n"); return 1; }
    if (!write_u16(1)) { DBG_MSG("SaveToBMP: failed writing planes\n"); return 1; }
    if (!write_u16(static_cast<uint16_t>(bpp))) { DBG_MSG("SaveToBMP: failed writing bitsPerPixel\n"); return 1; }
    if (!write_u32(0)) { DBG_MSG("SaveToBMP: failed writing compression\n"); return 1; } // BI_RGB
    if (!write_u32(static_cast<uint32_t>(imageSize))) { DBG_MSG("SaveToBMP: failed writing imageSize\n"); return 1; }
    if (!write_s32(0)) { DBG_MSG("SaveToBMP: failed writing xPelsPerMeter\n"); return 1; }
    if (!write_s32(0)) { DBG_MSG("SaveToBMP: failed writing yPelsPerMeter\n"); return 1; }
    if (!write_u32(0)) { DBG_MSG("SaveToBMP: failed writing clrUsed\n"); return 1; }
    if (!write_u32(0)) { DBG_MSG("SaveToBMP: failed writing clrImportant\n"); return 1; }

    // --- Pixels (BGR, bottom-up: write from bottom row to top) ---
    std::vector<unsigned char> row(rowSize, 0);
    for (int y = height - 1; y >= 0; --y)
    {
        // fill BGR row (no alpha)
        for (int x = 0; x < width; ++x)
        {
            unsigned char idx = pixels[y * width + x];
            row[x * 3 + 0] = palette[idx].b;
            row[x * 3 + 1] = palette[idx].g;
            row[x * 3 + 2] = palette[idx].r;
        }
        // padding has already been set to 0 during row initialization
        if (dst.write(row.data(), rowSize) != static_cast<std::size_t>(rowSize))
        {
            DBG_MSG("SaveToBMP: failed writing pixel row %d\n", y);
            return 1;
        }
    }

    // Reset stream position to start (host expects this)
    if (!dst.seek_abs(0)) { DBG_MSG("SaveToBMP: seek_abs(0) failed\n"); return 1; }

#ifdef _DEBUG
    char dbgBuf[256];
    _snprintf_s(dbgBuf, sizeof(dbgBuf), _TRUNCATE, "SaveToBMP: OK (24bpp, %dx%d, fileSize=%u)\n", width, height, fileSize);
    DBG_MSG(dbgBuf);
#endif

    return 0; // success
}

// ===================================================================
// Save as TGA (always 8bpp paletted, palette 24bpp BGR,
// ignore transparency - index 0 drawn as black)
// We write the header field-by-field in little-endian to avoid issues
// with padding/align between compilers.
// Scope: internal (static)
// ===================================================================
static int SaveToTGA(DelphiTStreamWrapper &dst,
              const std::vector<unsigned char> &pixels,
              int width, int height,
              const Color *palette_src,
              bool useTransparency)
{
    // Copy palette and, if needed, zero index 0 (transparent -> black)
    Color palette[256];
    std::memcpy(palette, palette_src, sizeof(palette));
    if (useTransparency)
    {
        palette[0].r = 0;
        palette[0].g = 0;
        palette[0].b = 0;
    }

    // Parameters
    const uint8_t idLength = 0;
    const uint8_t colorMapType = 1;    // palette present
    const uint8_t imageType = 1;       // colormapped, uncompressed
    const uint16_t colorMapStart = 0;
    const uint16_t colorMapLength = 256;
    const uint8_t colorMapBits = 24;   // 3 bytes per palette entry (B,G,R)
    const uint16_t xOrigin = 0;
    const uint16_t yOrigin = 0;
    const uint16_t pixelDepth = 8;     // 8bpp indexed
    const uint8_t imageDesc = 0x20;    // bit5 = top-left origin

    // Little-endian helpers
    auto write_u8 = [&](uint8_t v) -> bool { return dst.write(&v, 1) == 1; };
    auto write_u16 = [&](uint16_t v) -> bool {
        unsigned char b[2] = { static_cast<unsigned char>(v & 0xFF),
                               static_cast<unsigned char>((v >> 8) & 0xFF) };
        return dst.write(b, 2) == 2;
    };

    // --- Write TGA header (18 bytes) ---
    if (!write_u8(idLength)) { DBG_MSG("SaveToTGA: failed writing idLength\n"); return 1; }
    if (!write_u8(colorMapType)) { DBG_MSG("SaveToTGA: failed writing colorMapType\n"); return 1; }
    if (!write_u8(imageType)) { DBG_MSG("SaveToTGA: failed writing imageType\n"); return 1; }
    if (!write_u16(colorMapStart)) { DBG_MSG("SaveToTGA: failed writing colorMapStart\n"); return 1; }
    if (!write_u16(colorMapLength)) { DBG_MSG("SaveToTGA: failed writing colorMapLength\n"); return 1; }
    if (!write_u8(colorMapBits)) { DBG_MSG("SaveToTGA: failed writing colorMapBits\n"); return 1; }
    if (!write_u16(xOrigin)) { DBG_MSG("SaveToTGA: failed writing xOrigin\n"); return 1; }
    if (!write_u16(yOrigin)) { DBG_MSG("SaveToTGA: failed writing yOrigin\n"); return 1; }
    if (!write_u16(static_cast<uint16_t>(width))) { DBG_MSG("SaveToTGA: failed writing width\n"); return 1; }
    if (!write_u16(static_cast<uint16_t>(height))) { DBG_MSG("SaveToTGA: failed writing height\n"); return 1; }
    if (!write_u8(pixelDepth)) { DBG_MSG("SaveToTGA: failed writing pixelDepth\n"); return 1; }
    if (!write_u8(imageDesc)) { DBG_MSG("SaveToTGA: failed writing imageDesc\n"); return 1; }

    // --- Write palette (256 * 3 = B,G,R) ---
    for (int i = 0; i < 256; ++i)
    {
        unsigned char bgr[3] = { palette[i].b, palette[i].g, palette[i].r };
        if (dst.write(bgr, 3) != 3) { DBG_MSG("SaveToTGA: failed writing palette entry %d\n", i); return 1; }
    }

    // --- Write pixel indices ---
    // imageDesc = 0x20 => top-left origin, so write rows 0..height-1 in natural order
    for (int y = 0; y < height; ++y)
    {
        const unsigned char *row = &pixels[y * width];
        if (dst.write(row, width) != static_cast<std::size_t>(width))
        {
            DBG_MSG("SaveToTGA: failed writing pixel row %d\n", y);
            return 1;
        }
    }

    // Reset stream position to start (host expects this)
    if (!dst.seek_abs(0)) { DBG_MSG("SaveToTGA: seek_abs(0) failed\n"); return 1; }

#ifdef _DEBUG
    // Direct DBG_MSG (formatted buffer, independent of macro)
    char dbgBuf[128];
    _snprintf_s(dbgBuf, sizeof(dbgBuf), _TRUNCATE, "SaveToTGA: OK (8bpp paletted, %dx%d)\n", width, height);
    DBG_MSG(dbgBuf);
#endif

    return 0; // success
}

// =========================================================================================
// Save as PNG (supports 8bpp paletted, 24bpp true-color and 32bpp RGBA)
// =========================================================================================
static int SaveToPNG(DelphiTStreamWrapper &dst,
                     const std::vector<unsigned char> &pixels,
                     int width, int height,
                     const Color *palette,
                     bool useTransparency)
{
    std::vector<char> png;
    const unsigned char signature[] = { 0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A };
    png.insert(png.end(), std::begin(signature), std::end(signature));

    // --- IHDR ---
    struct { unsigned int w, h; unsigned char bd, ct, cm, f, i; } ihdr = {};
    ihdr.w = _byteswap_ulong(static_cast<unsigned int>(width));
    ihdr.h = _byteswap_ulong(static_cast<unsigned int>(height));
    ihdr.bd = 8; // always 8 bits per channel
    ihdr.cm = 0; ihdr.f = 0; ihdr.i = 0;

    if (g_default_PNG_Mode == PNGMode::PNG_8)      ihdr.ct = 3; // indexed-color
    else if (g_default_PNG_Mode == PNGMode::PNG_24) ihdr.ct = 2; // true-color RGB
    else                              ihdr.ct = 6; // true-color RGBA

    write_PNG_Chunk(png, "IHDR", reinterpret_cast<unsigned char *>(&ihdr), 13);

    // --- PLTE / tRNS only for 8bpp ---
    if (g_default_PNG_Mode == PNGMode::PNG_8)
    {
        unsigned char plte[768];
        for (int i = 0; i < 256; ++i)
        {
            plte[i * 3 + 0] = palette[i].r;
            plte[i * 3 + 1] = palette[i].g;
            plte[i * 3 + 2] = palette[i].b;
        }
        write_PNG_Chunk(png, "PLTE", plte, 768);

        if (useTransparency)
        {
            unsigned char trns[256]; std::memset(trns, 255, 256); trns[0] = 0;
            write_PNG_Chunk(png, "tRNS", trns, 256);
        }
    }

    // --- IDAT ---
    std::vector<unsigned char> idat;
    if (g_default_PNG_Mode == PNGMode::PNG_8)
    {
        idat.reserve(pixels.size() + height);
        for (int y = 0; y < height; ++y)
        {
            idat.push_back(0); // filter
            idat.insert(idat.end(),
                        pixels.begin() + y * width,
                        pixels.begin() + (y + 1) * width);
        }
    }
    else if (g_default_PNG_Mode == PNGMode::PNG_24)
    {
        idat.reserve(width * height * 3 + height);
        for (int y = 0; y < height; ++y)
        {
            idat.push_back(0); // filter
            for (int x = 0; x < width; ++x)
            {
                unsigned char idx = pixels[y * width + x];
                idat.push_back(palette[idx].r);
                idat.push_back(palette[idx].g);
                idat.push_back(palette[idx].b);
            }
        }
    }
    else // PNG_32
    {
        idat.reserve(width * height * 4 + height);
        for (int y = 0; y < height; ++y)
        {
            idat.push_back(0); // filter
            for (int x = 0; x < width; ++x)
            {
                unsigned char idx = pixels[y * width + x];
                idat.push_back(palette[idx].r);
                idat.push_back(palette[idx].g);
                idat.push_back(palette[idx].b);
                idat.push_back(useTransparency && idx == 0 ? 0 : 255); // alpha
            }
        }
    }

    // --- deflate compression ---
    z_stream zs = {};
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) { DBG_MSG("SaveToPNG: deflateInit failed\n"); return 1; }
    std::vector<unsigned char> compressed(deflateBound(&zs, (uLong)idat.size()) + 16);
    zs.next_in = idat.data(); zs.avail_in = (uInt)idat.size();
    zs.next_out = compressed.data(); zs.avail_out = (uInt)compressed.size();
    deflate(&zs, Z_FINISH);
    size_t compSize = compressed.size() - zs.avail_out;
    deflateEnd(&zs);

    write_PNG_Chunk(png, "IDAT", compressed.data(), static_cast<unsigned int>(compSize));
    write_PNG_Chunk(png, "IEND", nullptr, 0);

    dst.write(png.data(), png.size());

#ifdef _DEBUG
    char dbgBuf[256];
    const char *modeStr = (g_default_PNG_Mode == PNGMode::PNG_8) ? "8bpp paletted" :
        (g_default_PNG_Mode == PNGMode::PNG_24) ? "24bpp true-color" :
        "32bpp RGBA";
    _snprintf_s(dbgBuf, sizeof(dbgBuf), _TRUNCATE,
                "SaveToPNG: OK (%s, %dx%d)\n", modeStr, width, height);
    DBG_MSG(dbgBuf);
#endif // _DEBUG

    return 0;
}

// ===================================================================
// Convert PID using stream abstraction (DUCI-compatible – no direct TStream calls)
// ===================================================================
extern "C" int __stdcall ConvertPID(void *src, void *dst, const char *cnv)
{
    DBG_MSG("ConvertPID: called\n");

    try
    {
        DelphiTStreamWrapper srcStream(src);
        DelphiTStreamWrapper dstStream(dst);

        // Read PID header
        PIDHeader header;
        if (srcStream.read(&header, sizeof(header)) != sizeof(header))
        {
            DBG_MSG("ConvertPID: failed reading PIDHeader\n");
            return 1;
        }
        DBG_MSG("ConvertPID: PIDHeader read OK\n");

        // Header validation
        if (header.ID != 10)
        {
            DBG_MSG("ConvertPID: header.ID != 10\n");
            return 1;
        }
        if (header.Width <= 0 || header.Height <= 0)
        {
            DBG_MSG("ConvertPID: invalid dimensions\n");
            return 1;
        }
        DBG_MSG("ConvertPID: header validation OK (W=%d H=%d)\n", header.Width, header.Height);

        std::size_t pixel_count = static_cast<std::size_t>(header.Width) * static_cast<std::size_t>(header.Height);
        if (pixel_count > (1ULL << 30))
        {
            DBG_MSG("ConvertPID: pixel_count too large\n");
            return 1;
        }

        bool useTransparency = (header.Flags & 0x01) != 0;
        bool mirror = (header.Flags & 0x08) != 0;
        bool invert = (header.Flags & 0x10) != 0;
        bool rleCompression = (header.Flags & 0x20) != 0;
        bool hasPalette = (header.Flags & 0x80) != 0;

        // Palette
        Color palette[256];
        if (hasPalette)
        {
            if (srcStream.seek(-768, TSeekOrigin::soFromEnd) < 0)
            {
                DBG_MSG("ConvertPID: seek to palette failed\n");
                return 1;
            }
            DBG_MSG("ConvertPID: seeking to palette OK\n");

            for (int i = 0; i < 256; ++i)
            {
                if (srcStream.read(&palette[i].r, 1) != 1 ||
                    srcStream.read(&palette[i].g, 1) != 1 ||
                    srcStream.read(&palette[i].b, 1) != 1)
                {
                    DBG_MSG("ConvertPID: reading palette failed at index %d\n", i);
                    return 1;
                }
                palette[i].a = 255;
                if (useTransparency && i == 0) palette[i].a = 0;
            }
            DBG_MSG("ConvertPID: palette read OK\n");
        }
        else
        {
            std::memcpy(palette, defaultPalette, sizeof(palette));
            if (useTransparency) palette[0].a = 0;
            DBG_MSG("ConvertPID: using default palette\n");
        }

        // Back to pixel data (offset 32)
        if (srcStream.seek(32, TSeekOrigin::soFromBeginning) < 0)
        {
            DBG_MSG("ConvertPID: seek to pixel data failed\n");
            return 1;
        }
        DBG_MSG("ConvertPID: seek to pixel data OK\n");

        // Decompression
        std::vector<unsigned char> pixels(pixel_count);
        std::size_t pos = 0;
        unsigned char A = 0, B = 0;

        if (rleCompression)
        {
            DBG_MSG("ConvertPID: rleCompression = true\n");
            while (pos < pixels.size())
            {
                if (srcStream.read(&A, 1) != 1) { DBG_MSG("ConvertPID: RLE read A failed\n"); break; }
                if (A > 128)
                {
                    int count = A - 128;
                    unsigned char fill = 0;
                    for (int i = 0; i < count && pos < pixels.size(); ++i)
                        pixels[pos++] = fill;
                }
                else
                {
                    for (int i = 0; i < A && pos < pixels.size(); ++i)
                    {
                        if (srcStream.read(&B, 1) != 1) { DBG_MSG("ConvertPID: RLE read B failed\n"); pos = SIZE_MAX; break; }
                        pixels[pos++] = B;
                    }
                    if (pos == SIZE_MAX) break;
                }
            }
        }
        else
        {
            DBG_MSG("ConvertPID: rleCompression = false\n");
            while (pos < pixels.size())
            {
                if (srcStream.read(&A, 1) != 1) { DBG_MSG("ConvertPID: raw read A failed\n"); break; }
                int count = (A > 192) ? (A - 192) : 1;
                if (A > 192)
                {
                    if (srcStream.read(&B, 1) != 1) { DBG_MSG("ConvertPID: raw read B failed\n"); break; }
                }
                else B = A;
                for (int i = 0; i < count && pos < pixels.size(); ++i)
                    pixels[pos++] = B;
            }
        }

        if (pos != pixels.size())
        {
            DBG_MSG("ConvertPID: decompression produced wrong size (got=%zu expected=%zu)\n", pos, pixels.size());
            return 1;
        }
        DBG_MSG("ConvertPID: decompression OK\n");

        // Mirror / invert
        if (mirror || invert)
        {
            DBG_MSG("ConvertPID: mirror=%d invert=%d\n", mirror ? 1 : 0, invert ? 1 : 0);
            std::vector<unsigned char> flipped(pixels.size());
            for (int y = 0; y < header.Height; ++y)
            {
                int srcY = invert ? (header.Height - 1 - y) : y;
                for (int x = 0; x < header.Width; ++x)
                {
                    int srcX = mirror ? (header.Width - 1 - x) : x;
                    flipped[y * header.Width + x] = pixels[srcY * header.Width + srcX];
                }
            }
            pixels = std::move(flipped);
            DBG_MSG("ConvertPID: mirror/invert applied\n");
        }

        // Write to target format
        int res = 1;
        if (std::strcmp(cnv, "BMP") == 0)
        {
            DBG_MSG("ConvertPID: target BMP\n");
            res = SaveToBMP(dstStream, pixels, header.Width, header.Height, palette, useTransparency);
        }
        else if (std::strcmp(cnv, "TGA8") == 0 || std::strcmp(cnv, "TGA") == 0)
        {
            DBG_MSG("ConvertPID: target TGA\n");
            res = SaveToTGA(dstStream, pixels, header.Width, header.Height, palette, useTransparency);
        }
        else if (std::strcmp(cnv, "PNG") == 0)
        {
            DBG_MSG("ConvertPID: target PNG\n");
            res = SaveToPNG(dstStream, pixels, header.Width, header.Height, palette, useTransparency);
        }
        else
        {
            DBG_MSG("ConvertPID: unsupported target format: %s\n", cnv ? cnv : "(null)");
            return 1;
        }

        if (res != 0)
        {
            DBG_MSG("ConvertPID: SaveToX failed (res=%d)\n", res);
            return 1;
        }

        DBG_MSG("ConvertPID: success\n");
        return 0;
    }
    catch (...)
    {
        DBG_MSG("ConvertPID: exception caught\n");
        return 1;
    }
}

// ===================================================================
// Exported: Minimal DUCI version (compatible with 3)
// ===================================================================
extern "C" BYTE __stdcall DUCIVersion()
{
    return 3;
}

// ===================================================================
// Exported: Current DUCI version (4)
// ===================================================================
extern "C" BYTE __stdcall DUCIVersionEx(BYTE supported)
{
    (void)supported; // ignore host-supported version
    return 4;
}

// ===================================================================
// Exported: Plugin version info
// ===================================================================
extern "C" __declspec(dllexport) ConvertInfoRec __stdcall VersionInfo2()
{
    return pluginInfo; // copied by value to the host
}

// ===================================================================
// Exported: Check if file is compatible (only .pid extension, independent of fmt)
// ===================================================================
extern "C" DBOOL __stdcall IsFileCompatible(const ShortString *nam_ss, INT64 Offset, INT64 Size, const ShortString *fmt_ss, int DataX, int DataY)
{
    (void)Offset; (void)Size; (void)DataX; (void)DataY;
    std::string nam = ShortStringPtrToString(nam_ss);
    std::string fmt = ShortStringPtrToString(fmt_ss);
    if (nam.empty()) return FALSE;
    size_t dot_pos = nam.rfind('.');
    if (dot_pos == std::string::npos) return FALSE;
    std::string ext = nam.substr(dot_pos);
    bool isCompatible = (_stricmp(ext.c_str(), ".pid") == 0);
#ifdef _DEBUG
    std::string msg = "IsFileCompatible(): " + std::string(isCompatible ? "True" : "False") + " (nam=" + nam + ", fmt=" + fmt + ")";
    OutputDebugStringA((msg + "\n").c_str());
#endif
    return isCompatible ? TRUE : FALSE;
}

// ===================================================================
// Exported: List of conversion options (only for .pid files)
// ===================================================================
extern "C" __declspec(dllexport) ConvertList __stdcall GetFileConvert(
    const ShortString *nam_ss, __int64 Offset, __int64 Size, const ShortString *fmt_ss, int DataX, int DataY)
{
    ConvertList result;
    std::memset(&result, 0, sizeof(ConvertList));

    std::string nam = ShortStringPtrToString(nam_ss);
    std::string fmt = ShortStringPtrToString(fmt_ss);

#ifdef _DEBUG
    std::string msg = "GetFileConvert() - DEBUG\n\n"
        "File: " + nam + "\n"
        "Offset: " + std::to_string(Offset) + "\n"
        "Size: " + std::to_string(Size) + "\n"
        "fmt: " + fmt + "\n"
        "DataX: " + std::to_string(DataX) + "\n"
        "DataY: " + std::to_string(DataY) + "\n\n"
        "Archive format: " + fmt;
    DBG_MSG((msg + "\n").c_str());
#endif

    // Check extension
    size_t dot_pos = nam.rfind('.');
    if (dot_pos == std::string::npos || _stricmp(nam.substr(dot_pos).c_str(), ".PID") != 0)
    {
        DBG_MSG("GetFileConvert: not a .PID file, returning empty list\n");
        return result;
    }

    result.NumFormats = 3;

    WriteShortString(result.List[0].Display, "BMP - Windows Bitmap (24bpp)");
    WriteShortString(result.List[0].Ext, "bmp");
    WriteShortString(result.List[0].ID, "BMP");

    WriteShortString(result.List[1].Display, "TGA - Targa (8bpp Colormap)");
    WriteShortString(result.List[1].Ext, "tga");
    WriteShortString(result.List[1].ID, "TGA8");

    WriteShortString(result.List[2].Display, (std::string("PNG - Portable Network Graphics (") + std::to_string(static_cast<unsigned short>(g_default_PNG_Mode)) + "bpp)").c_str());
    WriteShortString(result.List[2].Ext, "png");
    WriteShortString(result.List[2].ID, "PNG");

#ifdef _DEBUG
    DBG_MSG("GetFileConvert: returning %d formats\n", (int)result.NumFormats);
#endif

    return result;
}

// ===================================================================
// Exported: Stream conversion (for .pid files, independent of fmt)
// ===================================================================
extern "C" __declspec(dllexport) int __stdcall ConvertStream(
    void *src, void *dst,
    const ShortString *nam_ss,
    const ShortString *fmt_ss,
    const ShortString *cnv_ss,
    long long Offset,
    int DataX, int DataY,
    DBOOL Silent)
{
    (void)Offset; (void)DataX; (void)DataY; (void)Silent;

    std::string nam = ShortStringPtrToString(nam_ss);
    std::string fmt = ShortStringPtrToString(fmt_ss);
    std::string cnv = ShortStringPtrToString(cnv_ss);

    if (nam.empty() || cnv.empty()) return 1;

#ifdef _DEBUG
    std::string msg = "ConvertStream called (cnv=" + cnv + ", nam=" + nam + ", fmt=" + fmt + ")";
    OutputDebugStringA((msg + "\n").c_str());
#endif

    // Returns 0 on success, non-zero on error
    return ConvertPID(src, dst, cnv.c_str());
}

// ===================================================================
// Exported: File-based conversion wrapper (for older DUCI)
// ===================================================================
extern "C" int __stdcall Convert(const ShortString *srcFile_ss, const ShortString *dstFile_ss,
                                 const ShortString *nam_ss, const ShortString *fmt_ss, const ShortString *cnv_ss, INT64 Offset, int DataX, int DataY, DBOOL Silent)
{
    // Not used — host should use ConvertStream
    // Could implement a simple wrapper to ConvertStream if needed
    return 1;
}

// ===================================================================
// Exported: Initialize plugin with callbacks from DU
// ===================================================================
extern "C" void __stdcall InitPlugin(TPercentCallback per, TLanguageCallback lngid,
                                     const ShortString *DUP5Path_ss, HANDLE AppHandle, void *AppOwner)
{
    Percent = per;
    DLNGStr = lngid;
    std::string dupPath = ShortStringPtrToString(DUP5Path_ss);
    if (!dupPath.empty()) strcpy_s(CurPath, sizeof(CurPath), dupPath.c_str());
    else CurPath[0] = 0;
    AHandle = reinterpret_cast<HWND>(AppHandle);
    AOwner = AppOwner;
}

// ===================================================================
// Exported: Extended initialization (unused in this plugin)
// ===================================================================
extern "C" void __stdcall InitPluginEx4(TMsgBoxCallback MsgBox)
{
    (void)MsgBox; // Required for DUCI v4, but not used here
}

// ConfigDlgProc: dialog procedure for the plugin's setup window,
// allowing the user to choose the default PNG export mode (8/24/32 bpp).
INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        // Mark the currently selected mode
        CheckRadioButton(hDlg, IDC_RADIO_PNG8, IDC_RADIO_PNG32,
                         (g_default_PNG_Mode == PNGMode::PNG_8) ? IDC_RADIO_PNG8 :
                         (g_default_PNG_Mode == PNGMode::PNG_24) ? IDC_RADIO_PNG24 :
                         IDC_RADIO_PNG32);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (IsDlgButtonChecked(hDlg, IDC_RADIO_PNG8) == BST_CHECKED)
                g_default_PNG_Mode = PNGMode::PNG_8;
            else if (IsDlgButtonChecked(hDlg, IDC_RADIO_PNG24) == BST_CHECKED)
                g_default_PNG_Mode = PNGMode::PNG_24;
            else if (IsDlgButtonChecked(hDlg, IDC_RADIO_PNG32) == BST_CHECKED)
                g_default_PNG_Mode = PNGMode::PNG_32;

            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// ===================================================================
// Exported: Configuration dialog
// ===================================================================
extern "C" void __stdcall ConfigBox()
{
#if 0
    // simple message
    MessageBoxA(AHandle ? AHandle : GetActiveWindow(),
                "No configuration options for this plugin.",
                "Configuration", MB_OK | MB_ICONINFORMATION);
#else
    HMODULE hMod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCSTR>(&ConfigBox),
        &hMod);

    if (!hMod)
    {
        MessageBoxA(AHandle ? AHandle : GetActiveWindow(),
                    "Cannot get module handle for plugin DLL.",
                    "ConfigBox error", MB_OK | MB_ICONERROR);
        return;
    }

    INT_PTR ret = DialogBoxParamA(
        hMod,
        MAKEINTRESOURCEA(IDD_PLUGIN_SETUP1),
        AHandle ? AHandle : GetActiveWindow(),
        ConfigDlgProc,
        0);

    if (ret == -1)
    {
        char buf[128];
        wsprintfA(buf, "DialogBoxParam failed. GetLastError=%lu", GetLastError());
        MessageBoxA(AHandle ? AHandle : GetActiveWindow(),
                    buf, "ConfigBox error", MB_OK | MB_ICONERROR);
    }


#endif
}

// ===================================================================
// Exported: About dialog
// ===================================================================
extern "C" void __stdcall AboutBox()
{
    MessageBoxA(AHandle ? AHandle : GetActiveWindow(),
                MSG_about.data(),
                "About .PID converter",
                MB_OK | MB_ICONINFORMATION);
}

// ===================================================================
// DLL entry point (unchanged)
// ===================================================================
#ifdef _DEBUG
// Verify structure sizes
static_assert(sizeof(ShortString) == 256, "ShortString size mismatch");
static_assert(sizeof(ConvertInfoRec) == 1028, "ConvertInfoRec size mismatch");
static_assert(sizeof(ConvertListElem) == 768, "ConvertListElem size mismatch");

// Log on attach (debug)
DBOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        char buf[256];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                    "=== PID Plugin Loaded ===\n"
                    "sizeof(ShortString)=%zu\n"
                    "sizeof(ConvertInfoRec)=%zu\n"
                    "sizeof(ConvertList)=%zu\n",
                    sizeof(ShortString), sizeof(ConvertInfoRec), sizeof(ConvertList));
        OutputDebugStringA(buf);
    }
    return TRUE;
}
#else
DBOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)hModule; (void)ul_reason_for_call; (void)lpReserved;
    return TRUE;
}
#endif