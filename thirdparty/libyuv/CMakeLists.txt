cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_VERBOSE_MAKEFILE on)

set(YUV_DIR ${CMAKE_CURRENT_LIST_DIR})
set(yuv_source_DIR ${YUV_DIR}/source)
set(yuv_include_DIR ${YUV_DIR}/include)

add_library(
    libyuv
    STATIC 
    ${yuv_source_DIR}/compare.cc
    ${yuv_source_DIR}/compare_common.cc
    ${yuv_source_DIR}/compare_neon.cc
    ${yuv_source_DIR}/compare_neon64.cc
    ${yuv_source_DIR}/compare_posix.cc
    ${yuv_source_DIR}/compare_win.cc
    ${yuv_source_DIR}/convert.cc
    ${yuv_source_DIR}/convert_argb.cc
    ${yuv_source_DIR}/convert_from.cc
    ${yuv_source_DIR}/convert_from_argb.cc
    ${yuv_source_DIR}/convert_jpeg.cc
    ${yuv_source_DIR}/convert_to_argb.cc
    ${yuv_source_DIR}/convert_to_i420.cc
    ${yuv_source_DIR}/cpu_id.cc
    ${yuv_source_DIR}/format_conversion.cc
    ${yuv_source_DIR}/mjpeg_decoder.cc
    ${yuv_source_DIR}/mjpeg_validate.cc
    ${yuv_source_DIR}/planar_functions.cc
    ${yuv_source_DIR}/rotate.cc
    ${yuv_source_DIR}/rotate_argb.cc
    ${yuv_source_DIR}/rotate_mips.cc
    ${yuv_source_DIR}/rotate_neon.cc
    ${yuv_source_DIR}/rotate_neon64.cc
    ${yuv_source_DIR}/row_any.cc
    ${yuv_source_DIR}/row_common.cc
    ${yuv_source_DIR}/row_mips.cc
    ${yuv_source_DIR}/row_neon.cc
    ${yuv_source_DIR}/row_neon64.cc
    ${yuv_source_DIR}/row_posix.cc
    ${yuv_source_DIR}/row_win.cc
    ${yuv_source_DIR}/scale.cc
    ${yuv_source_DIR}/scale_argb.cc
    ${yuv_source_DIR}/scale_common.cc
    ${yuv_source_DIR}/scale_mips.cc
    ${yuv_source_DIR}/scale_neon.cc
    ${yuv_source_DIR}/scale_neon64.cc
    ${yuv_source_DIR}/scale_posix.cc
    ${yuv_source_DIR}/scale_win.cc
    ${yuv_source_DIR}/video_common.cc
)

target_include_directories(libyuv PUBLIC ${yuv_include_DIR})

