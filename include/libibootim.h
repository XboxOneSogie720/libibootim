/*-
 * Copyright 2026 Karson Eskind
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIBIBOOTIM_H
#define LIBIBOOTIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    IBOOTIM_TYPE_PNG,
    IBOOTIM_TYPE_MODERN,
    IBOOTIM_TYPE_LEGACY, // Only matters during loading. After ctx is populated, IBOOTIM_TYPE_MODERN and IBOOTIM_TYPE_LEGACY are interchangeable as serialization is the same.
    IBOOTIM_TYPE_UNKNOWN = -255
} ibootim_type_t;

typedef enum {
    IBOOTIM_COLORSPACE_GRAYSCALE = 0x67726579,
    IBOOTIM_COLORSPACE_ARGB      = 0x61726762,
    IBOOTIM_COLORSPACE_UNKNOWN   = -255
} ibootim_colorspace_t;

typedef enum {
    IBOOTIM_E_SUCCESS                 = 0,
    IBOOTIM_E_NULL_INPUT_BUFFER       = -1,
    IBOOTIM_E_BUFFER_SIZE_TOO_SMALL   = -2,
    IBOOTIM_E_CTX_INVALID             = -3,
    IBOOTIM_E_UNKNOWN_IMAGE_TYPE      = -4,
    IBOOTIM_E_NO_MEM                  = -5,
    IBOOTIM_E_INVALID_FILENAME        = -6,
    IBOOTIM_E_FILE_OPEN_FAILED        = -7,
    IBOOTIM_E_FILE_READ_FAILED        = -8,
    IBOOTIM_E_LZSS_XPRESSION_ERROR    = -9,
    IBOOTIM_E_PNG_HANDLING_FAILED     = -10,
    IBOOTIM_E_PNG_INVALID             = -11,
    IBOOTIM_E_IMAGE_SIGNATURE_INVALID = -12,
    IBOOTIM_E_BAD_POINTER             = -13,
    IBOOTIM_E_INVALID_TYPE            = -14,
    IBOOTIM_E_FILE_WRITE_FAILED       = -15,
    IBOOTIM_E_MULTIPLE_SUB_IMAGES     = -16,
    IBOOTIM_E_INDEX_OUT_OF_RANGE      = -17
} ibootim_error_t;

typedef struct ibootim_ctx* ibootim_ctx_t;

/**
 * @brief Gets an error message for the given error code.
 * @param[in] error The error code to reference.
 * @return A human-readable error message.
 */
const char* ibootim_strerror(ibootim_error_t error);

/**
 * @brief Counts the number of images within an ibootim file.
 * @param[in] buffer Buffer to count from.
 * @param[in] buffer_size Size of the input buffer.
 * @param[out] images_count Number of images inside.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_count_images_in_buffer(uint8_t* buffer, size_t buffer_size, size_t* images_count);

/**
 * @brief Counts the number of images within an ibootim file.
 * @param[in] filename Path to the file to count from.
 * @param[out] images_count Number of images inside.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_count_images_in_file(const char* filename, size_t* images_count);

/**
 * @brief Loads and decompresses an ibootim image from a buffer.
 * @param[in] buffer Buffer to load from.
 * @param[in] buffer_size Size of the input buffer.
 * @param[in] index Index to load at. 0 will always be a valid index as long as the image itself is good.
 * @param[out] ctx Pointer to your ctx object to store the loaded file in.
 * @return An ibootim_error_t error code.
 * @note The inputted buffer is duplicated, so you can do whatever you want with the pointer you provided to @param buffer.
 */
ibootim_error_t ibootim_load_from_buffer(uint8_t* buffer, size_t buffer_size, size_t index, ibootim_ctx_t* ctx);

/**
 * @brief Loads and decompresses an ibootim image from a file.
 * @param[in] filename Path to the file to load from.
 * @param[in] index Index to load at. 0 will always be a valid index as long as the image itself is good.
 * @param[out] ctx Pointer to your ctx object to store the loaded file in.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_load_from_file(const char* filename, size_t index, ibootim_ctx_t* ctx);

/**
 * @brief Frees and nullifies an ibootim ctx.
 * @param[in] ctx Pointer to your ctx object to free and set to NULL.
 */
void ibootim_free_ctx(ibootim_ctx_t* ctx);

/**
 * @brief Gets the type of an ibootim ctx.
 * @param[in] ctx The loaded ctx.
 * @param[out] type Pointer to store the type at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_type(ibootim_ctx_t ctx, ibootim_type_t* type);

/**
 * @brief Sets the ibootim type.
 * @param[in] ctx The loaded ctx.
 * @param[in] type The new type to change to.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_set_type(ibootim_ctx_t ctx, ibootim_type_t type);

/**
 * @brief Gets the ibootim colorspace.
 * @param[in] ctx The loaded ctx.
 * @param[out] colorspace Pointer to store the colorspace at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_colorspace(ibootim_ctx_t ctx, ibootim_colorspace_t* colorspace);

/**
 * @brief Sets the ibootim colorspace.
 * @param[in] ctx The loaded ctx.
 * @param[in] colorspace The new colorspace to change to.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_set_colorspace(ibootim_ctx_t ctx, ibootim_colorspace_t colorspace);

/**
 * @brief Gets a string representation for the given type.
 * @param[in] type Type of image.
 * @return A string, duh.
 */
const char* type_to_str(ibootim_type_t type);

/**
 * @brief Gets a string representation for the given colorspace.
 * @param[in] type Type of image.
 * @return A string, duh.
 */
const char* colorspace_to_str(ibootim_colorspace_t colorspace);

/**
 * @brief Gets the ibootim width.
 * @param[in] ctx The loaded ctx.
 * @param[out] width Pointer to store the width at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_width(ibootim_ctx_t ctx, uint16_t* width);

/**
 * @brief Gets the ibootim height.
 * @param[in] ctx The loaded ctx.
 * @param[out] height Pointer to store the height at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_height(ibootim_ctx_t ctx, uint16_t* height);

/**
 * @brief Gets the ibootim's x-offset.
 * @param[in] ctx The loaded ctx.
 * @param[out] x_offset Pointer to store the x-offset at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_x_offset(ibootim_ctx_t ctx, int16_t* x_offset);

/**
 * @brief Sets the ibootim's x-offset.
 * @param[in] ctx The loaded ctx.
 * @param[in] x_offset The new x-offset to change to.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_set_x_offset(ibootim_ctx_t ctx, int16_t x_offset);

/**
 * @brief Gets the ibootim's y-offset.
 * @param[in] ctx The loaded ctx.
 * @param[out] y_offset Pointer to store the y-offset at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_y_offset(ibootim_ctx_t ctx, int16_t* y_offset);

/**
 * @brief Sets the ibootim's y-offset.
 * @param[in] ctx The loaded ctx.
 * @param[in] y_offset The new y-offset to change to.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_set_y_offset(ibootim_ctx_t ctx, int16_t y_offset);

/**
 * @brief Makes a copy of the raw pixel data inside the ibootim at the specified pointer location.
 * @param[in] ctx The loaded ctx.
 * @param[out] buffer Pointer to the buffer to allocate and copy data into.
 * @param[out] size Pointer to store the buffer's size at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_get_buffer(ibootim_ctx_t ctx, uint8_t** buffer, size_t* size);

/**
 * @brief Writes the ibootim image out to a buffer according to the internal type.
 * @param[in] ctx The loaded ctx.
 * @param[out] buffer Pointer to the buffer to write the serialized data to.
 * @param[out] size Pointer to store the buffer's size at.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_write_to_buffer(ibootim_ctx_t ctx, uint8_t** buffer, size_t* size);

/**
 * @brief Writes the ibootim image out to a file according to the internal type.
 * @param[in] ctx The loaded ctx.
 * @param[in] filename The filename to write to.
 * @return An ibootim_error_t error code.
 */
ibootim_error_t ibootim_write_to_file(ibootim_ctx_t ctx, const char* filename);

#ifdef __cplusplus
}
#endif

#endif
