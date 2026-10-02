/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

#include "debugdata.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <mutex>

#include "LzmaDec.h"
#include "xz.h"

namespace znn::debugdata {
namespace {

constexpr size_t kMaxDecompressedSize = 64u * 1024u * 1024u;

constexpr uint32_t kMaxDictionarySize = 64u * 1024u * 1024u;

constexpr size_t kInitialOutputSize = 256u * 1024u;

constexpr uint8_t kXzMagic[] = {0xFD, '7', 'z', 'X', 'Z', 0x00};

Backend g_backend = Backend::kLzma;

std::once_flag g_crc_tables;

void ensureCrcTables() {
    std::call_once(g_crc_tables, [] {
        xz_crc32_init();
#ifdef XZ_USE_CRC64
        xz_crc64_init();
#endif
    });
}

bool isXzStream(const uint8_t* data, size_t size) {
    return size >= sizeof(kXzMagic) && memcmp(data, kXzMagic, sizeof(kXzMagic)) == 0;
}

void* lzmaAlloc(ISzAllocPtr, size_t size) { return malloc(size); }

void lzmaFree(ISzAllocPtr, void* address) { free(address); }

const ISzAlloc g_lzma_alloc = {lzmaAlloc, lzmaFree};

bool lzmaAlone(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    if (size < 13 || isXzStream(data, size)) return false;

    uint64_t declared_size = 0;
    for (int i = 0; i < 8; ++i) declared_size |= static_cast<uint64_t>(data[5 + i]) << (8 * i);

    const bool sized = declared_size != 0 && declared_size != UINT64_MAX;
    if (sized && declared_size > kMaxDecompressedSize) return false;

    const uint8_t* payload = data + 13;
    const size_t payload_size = size - 13;

    size_t capacity = sized ? static_cast<size_t>(declared_size) : kInitialOutputSize;
    while (true) {
        out.resize(capacity);

        SizeT dest_len = capacity;
        SizeT src_len = payload_size;
        ELzmaStatus status;

        const SRes result =
            LzmaDecode(out.data(), &dest_len, payload, &src_len, data, 5,
                       sized ? LZMA_FINISH_END : LZMA_FINISH_ANY, &status, &g_lzma_alloc);
        if (result != SZ_OK) return false;

        if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
            status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) {
            out.resize(dest_len);
            return true;
        }

        if (sized || capacity >= kMaxDecompressedSize) return false;
        capacity = std::min(capacity * 2, kMaxDecompressedSize);
    }
}

bool xzStream(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    if (!isXzStream(data, size)) return false;

    ensureCrcTables();

    xz_dec* state = xz_dec_init(XZ_DYNALLOC, kMaxDictionarySize);
    if (!state) return false;

    out.resize(kInitialOutputSize);

    xz_buf buffer {};
    buffer.in = data;
    buffer.in_size = size;
    buffer.out = out.data();
    buffer.out_size = out.size();

    bool done = false;
    while (true) {
        const xz_ret result = xz_dec_run(state, &buffer);
        if (result == XZ_STREAM_END) {
            out.resize(buffer.out_pos);
            done = true;
            break;
        }
        if (result != XZ_OK || buffer.in_pos == buffer.in_size) break;
        if (buffer.out_pos < buffer.out_size) break;
        if (out.size() >= kMaxDecompressedSize) break;

        const size_t grown = std::min(out.size() * 2, kMaxDecompressedSize);
        out.resize(grown);
        buffer.out = out.data();
        buffer.out_size = grown;
    }

    xz_dec_end(state);
    return done;
}

bool tryBackend(Backend backend, const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    return backend == Backend::kXz ? xzStream(data, size, out) : lzmaAlone(data, size, out);
}

}  //namespace

Backend backend() { return g_backend; }

void setBackend(Backend value) { g_backend = value; }

bool parseBackend(const char* name, Backend* out) {
    if (!name || !out) return false;
    if (strcmp(name, "lzma") == 0) {
        *out = Backend::kLzma;
    } else if (strcmp(name, "xz") == 0) {
        *out = Backend::kXz;
    } else {
        return false;
    }
    return true;
}

const char* backendName(Backend value) {
    return value == Backend::kXz ? "xz" : "lzma";
}

bool decompress(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    out.clear();
    if (!data || size == 0) return false;

    const Backend preferred = backend();
    const Backend fallback = preferred == Backend::kXz ? Backend::kLzma : Backend::kXz;

    for (Backend candidate : {preferred, fallback}) {
        if (tryBackend(candidate, data, size, out)) return true;
        out.clear();
    }
    return false;
}

}  //namespace znn::debugdata
