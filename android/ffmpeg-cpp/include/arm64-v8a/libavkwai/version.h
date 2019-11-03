/*
 * Version macros.
 *
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVKWAI_VERSION_H
#define AVKWAI_VERSION_H

/**
 * @file
 */

#include "libavutil/version.h"

#define LIBAVKWAI_VERSION_MAJOR  1
#define LIBAVKWAI_VERSION_MINOR  0
#define LIBAVKWAI_VERSION_MICRO  1

#define LIBAVKWAI_VERSION_INT AV_VERSION_INT(LIBAVKWAI_VERSION_MAJOR, \
                                               LIBAVKWAI_VERSION_MINOR, \
                                               LIBAVKWAI_VERSION_MICRO)
#define LIBAVKWAI_VERSION     AV_VERSION(LIBAVKWAI_VERSION_MAJOR,   \
                                           LIBAVKWAI_VERSION_MINOR,   \
                                           LIBAVKWAI_VERSION_MICRO)
#define LIBAVKWAI_BUILD       LIBAVKWAI_VERSION_INT

#define LIBAVKWAI_IDENT       "Lavf" AV_STRINGIFY(LIBAVKWAI_VERSION)

#endif /* AVKWAI_VERSION_H */
