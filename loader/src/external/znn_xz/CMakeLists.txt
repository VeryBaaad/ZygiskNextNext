cmake_minimum_required(VERSION 3.22.1)
project(xz_embedded C)

set(XZ_EMBEDDED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../xz-embedded)

add_library(xz_embedded STATIC
    ${XZ_EMBEDDED_SRC}/linux/lib/xz/xz_crc32.c
    ${XZ_EMBEDDED_SRC}/linux/lib/xz/xz_crc64.c
    ${XZ_EMBEDDED_SRC}/linux/lib/xz/xz_dec_stream.c
    ${XZ_EMBEDDED_SRC}/linux/lib/xz/xz_dec_lzma2.c
    ${XZ_EMBEDDED_SRC}/linux/lib/xz/xz_dec_bcj.c
)
target_include_directories(xz_embedded
    PUBLIC ${XZ_EMBEDDED_SRC}/linux/include/linux
    PRIVATE ${XZ_EMBEDDED_SRC}/userspace
)
target_compile_definitions(xz_embedded
    PUBLIC XZ_USE_CRC64
    PRIVATE
        XZ_DEC_ARM
        XZ_DEC_ARMTHUMB
        XZ_DEC_ARM64
        XZ_DEC_RISCV
)
target_compile_options(xz_embedded PRIVATE -w)
