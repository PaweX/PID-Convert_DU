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
 *  FILE:       delphiTStreamWrapper.h
 *  AUTHOR:     Pawe³ C. (PaweX3)
 *  LICENSE:    MIT
 *
 *  BRIEF:      Safe C++ wrapper for the custom TStream in DU5
 *
 *  DETAILS:
 *      Defines a secure wrapper around the non-standard TStream used in
 *      Dragon UnPACKer 5. Provides VMT layout (vmt[0]=GetSize, vmt[3]=Read,
 *      vmt[5]=Seek), assembly wrappers (Call_ReadWrite, Call_Seek32,
 *      Call_GetSize) compatible with the Delphi ABI, and the
 *      DelphiTStreamWrapper class with methods read, write, seek, get_size,
 *      read_at – all with SEH protection and fBaseOffset handling for
 *      archive-contained files.
 * ============================================================================
 */

#pragma once
#ifndef DELPHI_TSTREAM_WRAPPER_H
#define DELPHI_TSTREAM_WRAPPER_H

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <limits>
#include <excpt.h>

// ------------------------------------------------------------------ //
// For Dragon UnPACKer 5:
// vmt[0] - returns file size
// vmt[1] - returns some number (maybe position of the file in the archive from the end?)
// vmt[2] - unexplored
// vmt[3] - read function
// vmt[4] - write function
// vmt[5] - seek function
// ------------------------------------------------------------------ //

enum class TSeekOrigin : std::uint16_t
{
    soFromBeginning = 0,
    soFromCurrent = 1,
    soFromEnd = 2
};


// ------------------------------------------------------------------ //
// Assembly wrappers compatible with Delphi register ABI (Win32/x86)
// Placed inside namespace DelphiABI
// ------------------------------------------------------------------ //
namespace DelphiABI
{

    // TStream.Read(Self, Buffer, Count: Longint): Longint
    __declspec(naked) int __stdcall Call_ReadWrite(void *self, void *buffer, int count, void *fn)
    {
        __asm {
            mov eax, [esp + 4]    // EAX = Self (pointer to TStream object)
            mov edx, [esp + 8]    // EDX = Buffer (address of buffer to write data into)
            mov ecx, [esp + 12]   // ECX = Count (how many bytes to read)
            mov ebx, [esp + 16]   // EBX = fn (address of Read method from VMT)
            call ebx              // call Delphi method: Read(Self, Buffer, Count)
            ret 16                // pop 4 arguments (4*4 bytes = 16) from stack
        }
    }

    // TStream.Seek(Self, Offset: Longint, Origin: Word): Longint
    __declspec(naked) int __stdcall Call_Seek32(void *self, int offset, unsigned short origin, void *fn)
    {
        __asm {
            mov eax, [esp + 4]    // EAX = Self
            mov edx, [esp + 8]    // EDX = Offset (Longint, offset)
            mov ecx, [esp + 12]   // ECX = Origin (Word, e.g. soFromBeginning=0)
            and ecx, 0xFFFF       // clear upper 16 bits of ECX (important because Delphi uses Word)
            mov ebx, [esp + 16]   // EBX = fn (address of Seek method from VMT)
            call ebx              // call Delphi method: Seek(Self, Offset, Origin)
            ret 16                // pop 4 arguments from stack
        }
    }

    // TStream.GetSize(Self): Longint
    __declspec(naked) int __stdcall Call_GetSize(void *self, void *fn)
    {
        __asm {
            mov eax, [esp + 4]    // EAX = Self
            mov ebx, [esp + 8]    // EBX = fn (address of GetSize method from VMT)
            call ebx              // call Delphi method: GetSize(Self)
            ret 8                 // pop 2 arguments from stack (self, fn)
        }
    }

    // SEH-wrapped safe calls
    inline int SafeReadWrite(void *self, void *buffer, int count, void *fn)
    {
        int result = -1;
        __try
        {
            result = Call_ReadWrite(self, buffer, count, fn);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = -1;
        }
        return result;
    }

    inline int SafeSeek32(void *self, int offset, unsigned short origin, void *fn)
    {
        int result = -1;
        __try
        {
            result = Call_Seek32(self, offset, origin, fn);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = -1;
        }
        return result;
    }

    inline int SafeGetSize(void *self, void *fn)
    {
        int result = -1;
        __try
        {
            result = Call_GetSize(self, fn);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = -1;
        }
        return result;
    }

} // namespace DelphiABI


// ------------------------------------------------------------------ //
// Class DelphiTStreamWrapper — uses DelphiABI:: to call VMT methods
// ------------------------------------------------------------------ //
class DelphiTStreamWrapper
{
private:
    struct VMT
    {
        // VMT registers:
        static constexpr unsigned int GetSize = 0; // function GetSize: Longint; virtual; abstract;                             // TESTED OK
        static constexpr unsigned int GetPos  = 1; // function GetPosition: Longint; virtual;                                   // ?---?---?
        static constexpr unsigned int SetSize = 2; // function SetSize(NewSize: Longint); virtual; abstract;                    // ?---?---?
        static constexpr unsigned int Read    = 3; // function Read(var Buffer; Count: Longint): Longint; virtual; abstract;    // TESTED OK
        static constexpr unsigned int Write   = 4; // function Write(const Buffer; Count: Longint): Longint; virtual; abstract; // TESTED OK
        static constexpr unsigned int Seek    = 5; // function Seek(Offset: Longint; Origin: Word): Longint; virtual; abstract; // TESTED OK

        // --- Additional methods (inherited/extended) ---
        // In Delphi 7 TStream also has non-virtual methods (e.g. CopyFrom, ReadBuffer, WriteBuffer),
        // but they are not in the VMT because they are implemented normally.
    };

    void *fStream;
    std::int64_t fBaseOffset;

    void **GetVMT() const { return *(void ***)fStream; }

public:
    explicit DelphiTStreamWrapper(void *streamPtr, std::int64_t initialOffset = 0)
        : fStream(streamPtr), fBaseOffset(0)
    {
        if (!fStream) throw std::invalid_argument("TStream pointer is null");

        if (initialOffset > 0)
        {
            auto vmt = GetVMT();
            void *seekPtr = vmt[VMT::Seek];
            if (!seekPtr) throw std::runtime_error("TStream.Seek function pointer is null");

            int pos = DelphiABI::SafeSeek32(fStream, static_cast<int>(initialOffset),
                                            static_cast<unsigned short>(TSeekOrigin::soFromBeginning), seekPtr);
            if (pos < 0) throw std::runtime_error("Seek to initialOffset failed");
            fBaseOffset = initialOffset;
        }
    }

    DelphiTStreamWrapper(const DelphiTStreamWrapper &) = delete;
    DelphiTStreamWrapper &operator=(const DelphiTStreamWrapper &) = delete;


    // ------------------------------------------------------------------
    // set_base_offset()
    // ------------------------------------------------------------------
    // Sets base offset (fBaseOffset) and moves the stream to that position.
    // - Parameters:
    //   newOffset : new base offset value (measured from the start of the stream).
    // - Returns: true if seek succeeded and offset was set,
    //            false on error (e.g. missing Seek pointer in VMT).
    // - Notes: useful when wrapper must handle files embedded inside an archive —
    //   then fBaseOffset points to the beginning of the embedded file.
    //   This adds semantics compared to a plain seek().
    bool set_base_offset(std::int64_t newOffset)
    {
        auto vmt = GetVMT();
        void *seekPtr = vmt[VMT::Seek];
        if (!seekPtr) return false;
        int pos = DelphiABI::SafeSeek32(fStream, static_cast<int>(newOffset),
                                        static_cast<unsigned short>(TSeekOrigin::soFromBeginning), seekPtr);
        if (pos < 0) return false;
        fBaseOffset = newOffset;
        return true;
    }

    // ------------------------------------------------------------------
    // position()
    // ------------------------------------------------------------------
    // Returns current stream position.
    // - Parameters: none.
    // - Returns: current position (>=0) or -1 on error.
    // - Notes: implementation = seek(0, soFromCurrent).
    //   Convenient alias for readability — use position() instead of seek(0, soFromCurrent).
    std::int64_t position() const
    {
        auto vmt = GetVMT();
        void *seekPtr = vmt[VMT::Seek];
        if (!seekPtr) return -1;
        int pos = DelphiABI::SafeSeek32(fStream, 0,
                                        static_cast<unsigned short>(TSeekOrigin::soFromCurrent), seekPtr);
        return static_cast<std::int64_t>(pos);
    }

    // ------------------------------------------------------------------
    // get_size()
    // ------------------------------------------------------------------
    // Calls GetSize method from the Delphi TStream VMT.
    // - Parameters: none (uses internal fStream pointer).
    // - Returns: stream size in bytes (>=0) or -1 on error.
    // - Notes: this is the native Delphi method — fast and correct, no "seek-hack" required.
    std::int64_t get_size()
    {
        auto vmt = GetVMT();
        void *sizePtr = vmt[VMT::GetSize];
        if (!sizePtr) return -1;
        int result = DelphiABI::SafeGetSize(fStream, sizePtr);
        return (result < 0) ? -1 : static_cast<std::int64_t>(result);
    }

    // ------------------------------------------------------------------
    // read()
    // ------------------------------------------------------------------
    // Calls Read method from Delphi TStream VMT.
    // - Parameters:
    //   buffer : pointer to the buffer where data will be read,
    //   count  : number of bytes to read.
    // - Returns: actual number of bytes read (>=0), 0 on error.
    // - Notes: defensively checks for nulls and clamps count to INT_MAX,
    //   because Delphi uses a 32-bit Count parameter.
    std::size_t read(void *buffer, std::size_t count)
    {
        if (!fStream || count == 0 || !buffer) return 0;

        auto vmt = GetVMT();
        void *readPtr = vmt[VMT::Read];
        if (!readPtr) return 0;

        int icount = (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            ? std::numeric_limits<int>::max()
            : static_cast<int>(count);

        int result = DelphiABI::SafeReadWrite(fStream, buffer, icount, readPtr);
        return (result < 0) ? 0 : static_cast<std::size_t>(result);
    }

    // ------------------------------------------------------------------
    // write()
    // ------------------------------------------------------------------
    // Calls Write method from Delphi TStream VMT.
    // - Parameters:
    //   buffer : pointer to data to write,
    //   count  : number of bytes to write.
    // - Returns: actual number of bytes written (>=0), 0 on error.
    // - Notes: Write ABI is identical to Read (Self->EAX, Buffer->EDX, Count->ECX).
    //   If the plugin doesn't write to host streams, this method might be unused.
    std::size_t write(const void *buffer, std::size_t count)
    {
        if (!fStream || count == 0 || !buffer) return 0;

        auto vmt = GetVMT();
        void *writePtr = vmt[VMT::Write];
        if (!writePtr) return 0;

        int icount = (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            ? std::numeric_limits<int>::max()
            : static_cast<int>(count);

        int result = DelphiABI::SafeReadWrite(const_cast<void *>(fStream),
                                         const_cast<void *>(buffer),
                                         icount,
                                         writePtr);
        return (result < 0) ? 0 : static_cast<std::size_t>(result);
    }

    // ------------------------------------------------------------------
    // seek()
    // ------------------------------------------------------------------
    // Calls Seek method from Delphi TStream VMT.
    // - Parameters:
    //   offset : offset (Longint),
    //   origin : reference point (soFromBeginning, soFromCurrent, soFromEnd).
    // - Returns: new position in the stream (>=0) or -1 on error.
    // - Notes: basic method to change stream position.
    std::int64_t seek(std::int64_t offset, TSeekOrigin origin)
    {
        if (!fStream) return -1;
        auto vmt = GetVMT();
        void *seekPtr = vmt[VMT::Seek];
        if (!seekPtr) return -1;

        int res = DelphiABI::SafeSeek32(fStream,
                                        static_cast<int>(offset),
                                        static_cast<unsigned short>(origin),
                                        seekPtr);
        return static_cast<std::int64_t>(res);
    }

    // ------------------------------------------------------------------
    // seek_abs()
    // ------------------------------------------------------------------
    // Convenience alias to seek() with origin = soFromBeginning.
    // - Parameters: absoluteOffset : absolute position from stream start.
    // - Returns: true if seek succeeded, false on error.
    // - Notes: improves readability; no new functionality.
    bool seek_abs(std::int64_t absoluteOffset)
    {
        auto vmt = GetVMT();
        void *seekPtr = vmt[VMT::Seek];
        if (!seekPtr) return false;
        int res = DelphiABI::SafeSeek32(fStream,
                                        static_cast<int>(absoluteOffset),
                                        static_cast<unsigned short>(TSeekOrigin::soFromBeginning),
                                        seekPtr);
        return res >= 0;
    }

    // ------------------------------------------------------------------
    // read_at()
    // ------------------------------------------------------------------
    // Reads data from a position relative to fBaseOffset.
    // - Parameters:
    //   relOffset : offset relative to fBaseOffset,
    //   buffer    : destination buffer,
    //   count     : number of bytes to read.
    // - Returns: number of bytes read or 0 on error.
    // - Notes: implementation = seek_abs(fBaseOffset + relOffset) + read().
    //   Useful when plugin frequently operates on files embedded in archives
    //   and fBaseOffset points to the start of the embedded file.
    std::size_t read_at(std::int64_t relOffset, void *buffer, std::size_t count)
    {
        auto vmt = GetVMT();
        void *seekPtr = vmt[VMT::Seek];
        if (!seekPtr) return 0;

        int pos = DelphiABI::SafeSeek32(fStream,
                                        static_cast<int>(fBaseOffset + relOffset),
                                        static_cast<unsigned short>(TSeekOrigin::soFromBeginning),
                                        seekPtr);
        if (pos < 0) return 0;

        return read(buffer, count);
    }
};

#endif // DELPHI_TSTREAM_WRAPPER_H
