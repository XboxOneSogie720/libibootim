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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <stdio.h>
#include <libibootim.h>
#include <stdbool.h>
#include <stddef.h>
#include "lzss.h"

#define IBOOTIM_MAGIC_IDENT            "iBootIm"
#define IBOOTIM_COMPRESSION_LZSS_IDENT 0x6c7a7373
#define LZSS_DECOMPRESS_WIGGLE_ROOM    256

typedef struct __attribute__((packed)) {
	uint8_t signature[8];
	uint32_t adler;
	uint32_t compression_type;
	uint32_t colorspace;
	uint16_t width;
	uint16_t height;
	int16_t x_offset;
	int16_t y_offset;
	uint32_t compressed_size;
	uint32_t reserved[8];
} ibootim_header_t;

typedef struct {
    uint8_t* data;
    size_t size;
} buffer_t;

struct ibootim_ctx {
    buffer_t working_buf;
    ibootim_type_t type;
    uint32_t compression_type;
    ibootim_colorspace_t colorspace;
    uint16_t width, height;
    int16_t x_offset, y_offset;
};

const char* ibootim_strerror(ibootim_error_t error) {
    switch (error) {
        case IBOOTIM_E_SUCCESS:
            return "Success.";
        case IBOOTIM_E_NULL_INPUT_BUFFER:
            return "The provided input buffer is NULL.";
        case IBOOTIM_E_BUFFER_SIZE_TOO_SMALL:
            return "The provided buffer size is too small to be valid.";
        case IBOOTIM_E_CTX_INVALID:
            return "The provided context is in an invalid state.";
        case IBOOTIM_E_UNKNOWN_IMAGE_TYPE:
            return "The provided image's type is unknown.";
        case IBOOTIM_E_NO_MEM:
            return "No memory available.";
        case IBOOTIM_E_INVALID_FILENAME:
            return "Null or invalid filename.";
        case IBOOTIM_E_FILE_OPEN_FAILED:
            return "Failed to open the provided file. Does it exist?";
        case IBOOTIM_E_FILE_READ_FAILED:
            return "Failed to read data from the file. Please try again.";
        case IBOOTIM_E_LZSS_XPRESSION_ERROR:
            return "An error occured during xpression of an LZSS payload.";
        case IBOOTIM_E_PNG_HANDLING_FAILED:
            return "An error occured while handling PNG data.";
        case IBOOTIM_E_PNG_INVALID:
            return "The provided PNG image is invalid.";
        case IBOOTIM_E_IMAGE_SIGNATURE_INVALID:
            return "The provided (modern) ibootim's signature is invalid.";
        case IBOOTIM_E_BAD_POINTER:
            return "The provided pointer is invalid.";
        case IBOOTIM_E_INVALID_TYPE:
            return "The provided type is not acceptable in the current context.";
        case IBOOTIM_E_FILE_WRITE_FAILED:
            return "Failed to write data out to the file.";
        case IBOOTIM_E_MULTIPLE_SUB_IMAGES:
            return "Multiple sub-images are not supported.";
        case IBOOTIM_E_INDEX_OUT_OF_RANGE:
            return "The provided index to load at is out of range for this image.";
        default:
            return "Unknown error.";
    }
}

static ibootim_type_t ibootim_get_type_from_buffer(uint8_t* buffer) {
    if (png_sig_cmp(buffer, 0, 8) == 0) return IBOOTIM_TYPE_PNG;
    if (strncmp((const char*)buffer, IBOOTIM_MAGIC_IDENT, strlen(IBOOTIM_MAGIC_IDENT)) != 0) return IBOOTIM_TYPE_UNKNOWN;

    ibootim_header_t* header = (ibootim_header_t*)buffer;
    if (header->compressed_size == 0) {
        return IBOOTIM_TYPE_LEGACY; // Legacy images always have this set to zero.
    } else {
        return IBOOTIM_TYPE_MODERN;
    }
}

static ibootim_error_t ibootim_get_offset_for_index(size_t index, uint8_t* buffer, size_t buffer_size, uint8_t** offsetted, size_t* offset_image_size) {
    size_t offset  = 0;
    size_t current = 0;

    while (true) {
        if (offset + sizeof(ibootim_header_t) > buffer_size) return IBOOTIM_E_INDEX_OUT_OF_RANGE;

        ibootim_header_t* header = (ibootim_header_t*)(buffer + offset);
        if (strncmp((const char*)header->signature, IBOOTIM_MAGIC_IDENT, strlen(IBOOTIM_MAGIC_IDENT)) != 0) return IBOOTIM_E_IMAGE_SIGNATURE_INVALID;

        if (current == index) {
            *offsetted = buffer + offset;
            *offset_image_size = (header->compressed_size == 0) ? buffer_size : sizeof(ibootim_header_t) + header->compressed_size;
            return IBOOTIM_E_SUCCESS;
        }

        if (header->compressed_size == 0) return IBOOTIM_E_INDEX_OUT_OF_RANGE;

        size_t seek = sizeof(ibootim_header_t) + header->compressed_size;
        if (offset + seek > buffer_size) return IBOOTIM_E_INDEX_OUT_OF_RANGE;

        offset += seek;
        current++;
    }
}

ibootim_error_t ibootim_count_images_in_buffer(uint8_t* buffer, size_t buffer_size, size_t* images_count) {
    if (buffer == NULL)                         return IBOOTIM_E_NULL_INPUT_BUFFER;
    if (buffer_size < sizeof(ibootim_header_t)) return IBOOTIM_E_BUFFER_SIZE_TOO_SMALL;
    if (images_count == NULL)                   return IBOOTIM_E_BAD_POINTER;

    ibootim_type_t type = ibootim_get_type_from_buffer(buffer);
    if (type == IBOOTIM_TYPE_UNKNOWN) return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;
    if (type == IBOOTIM_TYPE_PNG) {
        *images_count = 1;
        return IBOOTIM_E_SUCCESS;
    }

    size_t count     = 0;
    uint8_t* current = buffer;
    size_t remaining = buffer_size;

    while (remaining >= sizeof(ibootim_header_t)) {
        uint8_t* img_ptr = NULL;
        size_t img_size  = 0;

        ibootim_error_t error = ibootim_get_offset_for_index(0, current, remaining, &img_ptr, &img_size);
        if (error != IBOOTIM_E_SUCCESS) return error;

        count++;

        if (img_size == 0 || img_size >= remaining) break; // Legacy images only have 1 internal picture

        current   += img_size;
        remaining -= img_size;
    }

    *images_count = count;
    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_count_images_in_file(const char* filename, size_t* images_count) {
    if (filename == NULL)     return IBOOTIM_E_INVALID_FILENAME;
    if (images_count == NULL) return IBOOTIM_E_BAD_POINTER;

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) return IBOOTIM_E_FILE_OPEN_FAILED;

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* buf = malloc(file_size);
    if (buf == NULL) {
        fclose(fp);
        return IBOOTIM_E_NO_MEM;
    }

    if (fread(buf, 1, file_size, fp) != file_size) {
        fclose(fp);
        free(buf);
        return IBOOTIM_E_FILE_READ_FAILED;
    }
    fclose(fp);

    ibootim_error_t error = ibootim_count_images_in_buffer(buf, file_size, images_count);
    free(buf);
    
    return error;
}

static uint8_t ibootim_get_pixel_size_for_color_space(ibootim_colorspace_t colorSpace) {
	switch (colorSpace) {
		case IBOOTIM_COLORSPACE_ARGB:
			return 4;
		case IBOOTIM_COLORSPACE_GRAYSCALE:
			return 2;
		default:
			return 0;
	}
}

static ibootim_error_t ibootim_decompress_if_needed(ibootim_ctx_t ctx) {
    if (ctx->type == IBOOTIM_TYPE_PNG) return IBOOTIM_E_SUCCESS;

    if (ctx->compression_type != IBOOTIM_COMPRESSION_LZSS_IDENT) {
        // just strip header and return
        size_t data_size = ctx->working_buf.size - sizeof(ibootim_header_t);
        uint8_t* tmp = malloc(data_size);
        if (tmp == NULL) return IBOOTIM_E_NO_MEM;

        memcpy(tmp, ctx->working_buf.data + sizeof(ibootim_header_t), data_size);

        free(ctx->working_buf.data);
        ctx->working_buf.data = tmp;
        ctx->working_buf.size = data_size;

        return IBOOTIM_E_SUCCESS;
    }

    uint8_t pixel_size = ibootim_get_pixel_size_for_color_space(ctx->colorspace);
    if (pixel_size == 0) return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;

    size_t expected_uncompressed_size = (size_t)ctx->width * ctx->height * pixel_size + LZSS_DECOMPRESS_WIGGLE_ROOM;

    uint8_t* compressed_buf = ctx->working_buf.data + sizeof(ibootim_header_t);
    size_t compressed_size = ctx->working_buf.size - sizeof(ibootim_header_t);

    uint8_t* uncompressed_buf = malloc(expected_uncompressed_size);
    if (uncompressed_buf == NULL) return IBOOTIM_E_NO_MEM;

    int actual_uncompressed_size = decompress_lzss(uncompressed_buf, compressed_buf, compressed_size);
    if (actual_uncompressed_size <= 0 || (size_t)actual_uncompressed_size >= expected_uncompressed_size) {
        free(uncompressed_buf);
        return IBOOTIM_E_LZSS_XPRESSION_ERROR;
    }

    // realloc down to true size
    uint8_t* tmp = realloc(uncompressed_buf, actual_uncompressed_size);
    if (tmp == NULL) {
        free(uncompressed_buf);
        return IBOOTIM_E_NO_MEM;
    }

    uncompressed_buf = tmp;

    // replace ctx buffer with new uncompressed data
    free(ctx->working_buf.data);
    ctx->working_buf.data = uncompressed_buf;
    ctx->working_buf.size = actual_uncompressed_size;

    return IBOOTIM_E_SUCCESS;
}

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} png_buffer_t;

static void png_buffer_read(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count_to_read) {
    png_buffer_t *buf = (png_buffer_t *)png_get_io_ptr(png_ptr);

    if (buf->offset + byte_count_to_read > buf->size) {
        png_error(png_ptr, "Unexpected end of PNG buffer");
        return;
    }

    memcpy(out_bytes, buf->data + buf->offset, byte_count_to_read);
    buf->offset += byte_count_to_read;
}

static ibootim_error_t ibootim_load_png(uint8_t* buffer, size_t buffer_size, ibootim_ctx_t* ctx) {
    ibootim_ctx_t tmp_ctx = calloc(1, sizeof(struct ibootim_ctx));
    if (tmp_ctx == NULL) return IBOOTIM_E_NO_MEM;

    png_structp read_struct = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (read_struct == NULL) {
        free(tmp_ctx);
        return IBOOTIM_E_NO_MEM;
    }

    png_infop info_struct = png_create_info_struct(read_struct);
    if (info_struct == NULL) {
        png_destroy_read_struct(&read_struct, NULL, NULL);
        free(tmp_ctx);
        return IBOOTIM_E_NO_MEM;
    }

    png_bytep* rows = NULL;

    if (setjmp(png_jmpbuf(read_struct))) {
		png_destroy_read_struct(&read_struct, &info_struct, NULL);
        free(tmp_ctx->working_buf.data);
        free(tmp_ctx);
        free(rows);
		return IBOOTIM_E_PNG_HANDLING_FAILED;
	}

    png_buffer_t pngbuf = {
        .data = buffer,
        .size = buffer_size,
        .offset = 8
    };

    png_set_read_fn(read_struct, &pngbuf, png_buffer_read);
    png_set_sig_bytes(read_struct, 8);

    png_read_info(read_struct, info_struct);

    // Original image properties
    uint32_t width = png_get_image_width(read_struct, info_struct);
    uint32_t height = png_get_image_height(read_struct, info_struct);
    uint8_t color_type = png_get_color_type(read_struct, info_struct);
    uint8_t bit_depth = png_get_bit_depth(read_struct, info_struct);

    // Normalize the image
    if (bit_depth == 16) png_set_strip_16(read_struct);

    png_set_expand_gray_1_2_4_to_8(read_struct);
    png_set_palette_to_rgb(read_struct);
    png_set_tRNS_to_alpha(read_struct);

    if (!(color_type & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(read_struct, 0x00, PNG_FILLER_AFTER);

    png_set_invert_alpha(read_struct);

    if (color_type & PNG_COLOR_MASK_COLOR) png_set_bgr(read_struct);

    // Apply all requested transforms
    png_read_update_info(read_struct, info_struct);

    // Populate members
    tmp_ctx->type               = IBOOTIM_TYPE_PNG;
    tmp_ctx->compression_type   = IBOOTIM_COMPRESSION_LZSS_IDENT;

    switch (png_get_channels(read_struct, info_struct)) {
        case 4:
            tmp_ctx->colorspace = IBOOTIM_COLORSPACE_ARGB;
            break;

        case 2:
            tmp_ctx->colorspace = IBOOTIM_COLORSPACE_GRAYSCALE;
            break;

        default:
            tmp_ctx->colorspace = IBOOTIM_COLORSPACE_UNKNOWN;
            break;
    }

    tmp_ctx->width    = width;
    tmp_ctx->height   = height;
    tmp_ctx->x_offset = 0;
    tmp_ctx->y_offset = 0;

    // Prepare and read pixel data
    size_t rowbytes = png_get_rowbytes(read_struct, info_struct);

    if (height != 0 && rowbytes > SIZE_MAX / height) {
        png_destroy_read_struct(&read_struct, &info_struct, NULL);
        free(tmp_ctx);
        return IBOOTIM_E_PNG_INVALID;
    }

    tmp_ctx->working_buf.size = rowbytes * height;
    tmp_ctx->working_buf.data = malloc(tmp_ctx->working_buf.size);
    if (tmp_ctx->working_buf.data == NULL) {
        png_destroy_read_struct(&read_struct, &info_struct, NULL);
        free(tmp_ctx);
        return IBOOTIM_E_NO_MEM;
    }

    rows = malloc(height * sizeof(*rows));
    if (rows == NULL) {
        png_destroy_read_struct(&read_struct, &info_struct, NULL);
        free(tmp_ctx->working_buf.data);
        free(tmp_ctx);
        return IBOOTIM_E_NO_MEM;
    }

    for (uint32_t y = 0; y < height; y++) {
        rows[y] = tmp_ctx->working_buf.data + y * rowbytes;
    }

    png_read_image(read_struct, rows);

    free(rows);

    png_read_end(read_struct, NULL);
    png_destroy_read_struct(&read_struct, &info_struct, NULL);

    *ctx = tmp_ctx;
    return IBOOTIM_E_SUCCESS;
}

static unsigned adler32(unsigned adler, const unsigned char* data, unsigned len) {
	unsigned s1 = adler & 0xffff;
	unsigned s2 = (adler >> 16) & 0xffff;
	
	while(len > 0) {
		/*at least 5550 sums can be done before the sums overflow, saving a lot of module divisions*/
		unsigned amount = len > 5550 ? 5550 : len;
		len -= amount;
		while(amount > 0) {
			s1 += (*data++);
			s2 += s1;
			--amount;
		}
		s1 %= 65521;
		s2 %= 65521;
	}
	
	return (s2 << 16) | s1;
}

static bool ibootim_modern_crc32_signature_valid(uint8_t* buffer, size_t buffer_size) {
    ibootim_header_t* header = (ibootim_header_t*)buffer;

    if (sizeof(ibootim_header_t) + header->compressed_size > buffer_size) return false;

    uint8_t* compressed_data = buffer + sizeof(ibootim_header_t);
    
    unsigned header_adler = adler32(1, (unsigned char*)&header->compression_type, (unsigned int)(sizeof(ibootim_header_t) - offsetof(ibootim_header_t, compression_type)));
    unsigned image_adler  = adler32(header_adler, compressed_data, header->compressed_size);
    
    return header->adler == image_adler;
}

static ibootim_error_t ibootim_load_ibootim(uint8_t* buffer, size_t buffer_size, ibootim_type_t type, ibootim_ctx_t* ctx) {
    ibootim_ctx_t tmp_ctx = calloc(1, sizeof(struct ibootim_ctx));
    if (tmp_ctx == NULL) return IBOOTIM_E_NO_MEM;

    if (type == IBOOTIM_TYPE_MODERN && ibootim_modern_crc32_signature_valid(buffer, buffer_size) != true) return IBOOTIM_E_IMAGE_SIGNATURE_INVALID;

    tmp_ctx->working_buf.data = malloc(buffer_size);
    if (tmp_ctx->working_buf.data == NULL) {
        free(tmp_ctx);
        return IBOOTIM_E_NO_MEM;
    }

    ibootim_header_t* header = (ibootim_header_t*)buffer;
    memcpy(tmp_ctx->working_buf.data, buffer, buffer_size);
    tmp_ctx->type             = type;
    tmp_ctx->compression_type = header->compression_type;
    tmp_ctx->colorspace       = header->colorspace;
    tmp_ctx->width            = header->width;
    tmp_ctx->height           = header->height;
    tmp_ctx->x_offset         = header->x_offset;
    tmp_ctx->y_offset         = header->y_offset;
    tmp_ctx->working_buf.size = buffer_size;

    ibootim_error_t err = ibootim_decompress_if_needed(tmp_ctx);
    if (err != IBOOTIM_E_SUCCESS) {
        free(tmp_ctx->working_buf.data);
        free(tmp_ctx);
        return err;
    }

    *ctx = tmp_ctx;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_load_from_buffer(uint8_t* buffer, size_t buffer_size, size_t index, ibootim_ctx_t* ctx) {
    if (buffer == NULL)                         return IBOOTIM_E_NULL_INPUT_BUFFER;
    if (buffer_size < sizeof(ibootim_header_t)) return IBOOTIM_E_BUFFER_SIZE_TOO_SMALL;
    if (ctx == NULL || *ctx != NULL)            return IBOOTIM_E_CTX_INVALID;

    ibootim_type_t type = ibootim_get_type_from_buffer(buffer);
    if (type == IBOOTIM_TYPE_UNKNOWN) return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;

    uint8_t* load_ref    = buffer;
    size_t load_ref_size = buffer_size;

    if (type != IBOOTIM_TYPE_PNG) {
        ibootim_error_t error = ibootim_get_offset_for_index(index, buffer, buffer_size, &load_ref, &load_ref_size);
        if (error != IBOOTIM_E_SUCCESS) return error;
    }

    if (type == IBOOTIM_TYPE_PNG) {
        return ibootim_load_png(load_ref, load_ref_size, ctx);
    } else {
        return ibootim_load_ibootim(load_ref, load_ref_size, type, ctx);
    }
}

ibootim_error_t ibootim_load_from_file(const char* filename, size_t index, ibootim_ctx_t* ctx) {
    if (filename == NULL)            return IBOOTIM_E_INVALID_FILENAME;
    if (ctx == NULL || *ctx != NULL) return IBOOTIM_E_CTX_INVALID;

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) return IBOOTIM_E_FILE_OPEN_FAILED;

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* buf = malloc(size);
    if (buf == NULL) {
        fclose(fp);
        return IBOOTIM_E_NO_MEM;
    }

    if (fread(buf, 1, size, fp) != size) {
        fclose(fp);
        free(buf);
        return IBOOTIM_E_FILE_READ_FAILED;
    }
    fclose(fp);

    ibootim_error_t err = ibootim_load_from_buffer(buf, size, index, ctx);
    free(buf);
    
    return err;
}

void ibootim_free_ctx(ibootim_ctx_t* ctx) {
    if (ctx == NULL || *ctx == NULL) return;

    free((*ctx)->working_buf.data);
    free(*ctx);
    *ctx = NULL;
}

ibootim_error_t ibootim_get_type(ibootim_ctx_t ctx, ibootim_type_t* type) {
    if (ctx == NULL)  return IBOOTIM_E_CTX_INVALID;
    if (type == NULL) return IBOOTIM_E_BAD_POINTER;

    *type = ctx->type;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_set_type(ibootim_ctx_t ctx, ibootim_type_t type) {
    if (ctx == NULL) return IBOOTIM_E_CTX_INVALID;
    
    switch (type) {
        case IBOOTIM_TYPE_PNG:
        case IBOOTIM_TYPE_MODERN:
        case IBOOTIM_TYPE_LEGACY:
            ctx->type = type;
            break;
        default:
            return IBOOTIM_E_INVALID_TYPE;
    }

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_get_colorspace(ibootim_ctx_t ctx, ibootim_colorspace_t* colorspace) {
    if (ctx == NULL)        return IBOOTIM_E_CTX_INVALID;
    if (colorspace == NULL) return IBOOTIM_E_BAD_POINTER;

    *colorspace = ctx->colorspace;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_set_colorspace(ibootim_ctx_t ctx, ibootim_colorspace_t colorspace) {
    if (ctx == NULL) return IBOOTIM_E_CTX_INVALID;
    if (ctx->colorspace == colorspace) return IBOOTIM_E_SUCCESS;

    size_t pixel_count = (size_t)ctx->width * ctx->height;

    if (ctx->colorspace == IBOOTIM_COLORSPACE_ARGB && colorspace == IBOOTIM_COLORSPACE_GRAYSCALE) {
        uint8_t* new_buf = malloc(pixel_count * 2);
        if (new_buf == NULL) return IBOOTIM_E_NO_MEM;

        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t b = ctx->working_buf.data[i * 4 + 0];
            uint8_t g = ctx->working_buf.data[i * 4 + 1];
            uint8_t r = ctx->working_buf.data[i * 4 + 2];
            uint8_t a = ctx->working_buf.data[i * 4 + 3];

            // Standard luminance weights applied to the correct channels
            uint8_t y = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);

            new_buf[i * 2 + 0] = y;
            new_buf[i * 2 + 1] = a; // Alpha stays the same.
        }

        free(ctx->working_buf.data);
        ctx->working_buf.data = new_buf;
        ctx->working_buf.size = pixel_count * 2;
        ctx->colorspace       = IBOOTIM_COLORSPACE_GRAYSCALE;
        return IBOOTIM_E_SUCCESS;
    }

    if (ctx->colorspace == IBOOTIM_COLORSPACE_GRAYSCALE && colorspace == IBOOTIM_COLORSPACE_ARGB) {
        uint8_t* new_buf = malloc(pixel_count * 4);
        if (new_buf == NULL) return IBOOTIM_E_NO_MEM;

        for (size_t i = 0; i < pixel_count; i++) {
            uint8_t y = ctx->working_buf.data[i * 2 + 0];
            uint8_t a = ctx->working_buf.data[i * 2 + 1];

            // Expand to B, G, R, A. All color channels carry the same value
            new_buf[i * 4 + 0] = y;
            new_buf[i * 4 + 1] = y;
            new_buf[i * 4 + 2] = y;
            new_buf[i * 4 + 3] = a;
        }

        free(ctx->working_buf.data);
        ctx->working_buf.data = new_buf;
        ctx->working_buf.size = pixel_count * 4;
        ctx->colorspace       = IBOOTIM_COLORSPACE_ARGB;
        return IBOOTIM_E_SUCCESS;
    }

    return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;
}

const char* type_to_str(ibootim_type_t type) {
    switch (type) {
        case IBOOTIM_TYPE_PNG:
            return "PNG";
        case IBOOTIM_TYPE_MODERN:
            return "iBoot Image (Modern)";
        case IBOOTIM_TYPE_LEGACY:
            return "iBoot Image (Legacy)";
        default:
            return "Unknown";
    }
}

const char* colorspace_to_str(ibootim_colorspace_t colorspace) {
    switch (colorspace) {
        case IBOOTIM_COLORSPACE_GRAYSCALE:
            return "Grayscale";
        case IBOOTIM_COLORSPACE_ARGB:
            return "ARGB";
        default:
            return "Unknown";
    }
}

ibootim_error_t ibootim_get_width(ibootim_ctx_t ctx, uint16_t* width) {
    if (ctx == NULL)   return IBOOTIM_E_CTX_INVALID;
    if (width == NULL) return IBOOTIM_E_BAD_POINTER;

    *width = ctx->width;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_get_height(ibootim_ctx_t ctx, uint16_t* height) {
    if (ctx == NULL)    return IBOOTIM_E_CTX_INVALID;
    if (height == NULL) return IBOOTIM_E_BAD_POINTER;

    *height = ctx->height;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_get_x_offset(ibootim_ctx_t ctx, int16_t* x_offset) {
    if (ctx == NULL)      return IBOOTIM_E_CTX_INVALID;
    if (x_offset == NULL) return IBOOTIM_E_BAD_POINTER;

    *x_offset = ctx->x_offset;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_set_x_offset(ibootim_ctx_t ctx, int16_t x_offset) {
    if (ctx == NULL) return IBOOTIM_E_CTX_INVALID;

    ctx->x_offset = x_offset;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_get_y_offset(ibootim_ctx_t ctx, int16_t* y_offset) {
    if (ctx == NULL)      return IBOOTIM_E_CTX_INVALID;
    if (y_offset == NULL) return IBOOTIM_E_BAD_POINTER;

    *y_offset = ctx->y_offset;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_set_y_offset(ibootim_ctx_t ctx, int16_t y_offset) {
    if (ctx == NULL) return IBOOTIM_E_CTX_INVALID;

    ctx->y_offset = y_offset;

    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_get_buffer(ibootim_ctx_t ctx, uint8_t** buffer, size_t* size) {
    if (ctx == NULL)                                       return IBOOTIM_E_CTX_INVALID;
    if (buffer == NULL || *buffer != NULL || size == NULL) return IBOOTIM_E_BAD_POINTER;

    uint8_t* tmp = malloc(ctx->working_buf.size);
    if (tmp == NULL) return IBOOTIM_E_NO_MEM;

    memcpy(tmp, ctx->working_buf.data, ctx->working_buf.size);

    *buffer = tmp;
    *size   = ctx->working_buf.size;

    return IBOOTIM_E_SUCCESS;
}

typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
} png_write_buf_t;

static void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_write_buf_t* buf = (png_write_buf_t*)png_get_io_ptr(png_ptr);

    if (buf->size + length > buf->capacity) {
        size_t new_cap = buf->capacity * 2 + length;
        uint8_t* tmp = realloc(buf->data, new_cap);
        if (tmp == NULL) {
            png_error(png_ptr, "Out of memory in write callback");
            return;
        }
        buf->data     = tmp;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, data, length);
    buf->size += length;
}

static void png_flush_callback(png_structp png_ptr) {
    /* no-op for buffer writing */
    (void)png_ptr;
}

static ibootim_error_t ibootim_serialize_png(ibootim_ctx_t ctx, uint8_t** out_buf, size_t* out_size) {
    png_structp write_struct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (write_struct == NULL) return IBOOTIM_E_NO_MEM;

    png_infop info_struct = png_create_info_struct(write_struct);
    if (info_struct == NULL) {
        png_destroy_write_struct(&write_struct, NULL);
        return IBOOTIM_E_NO_MEM;
    }

    png_write_buf_t write_buf = { .data = malloc(4096), .size = 0, .capacity = 4096 };
    if (write_buf.data == NULL) {
        png_destroy_write_struct(&write_struct, &info_struct);
        return IBOOTIM_E_NO_MEM;
    }

    uint8_t* row_buf = NULL;

    if (setjmp(png_jmpbuf(write_struct))) {
        png_destroy_write_struct(&write_struct, &info_struct);
        free(write_buf.data);
        free(row_buf);
        return IBOOTIM_E_PNG_HANDLING_FAILED;
    }

    png_set_write_fn(write_struct, &write_buf, png_write_callback, png_flush_callback);

    int color_type = (ctx->colorspace == IBOOTIM_COLORSPACE_ARGB) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_GRAY_ALPHA;

    png_set_IHDR(write_struct, info_struct, ctx->width, ctx->height, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(write_struct, info_struct);

    uint8_t pixel_size = ibootim_get_pixel_size_for_color_space(ctx->colorspace);
    size_t  rowbytes   = (size_t)ctx->width * pixel_size;

    row_buf = malloc(rowbytes);
    if (row_buf == NULL) {
        png_destroy_write_struct(&write_struct, &info_struct);
        free(write_buf.data);
        return IBOOTIM_E_NO_MEM;
    }

    for (uint16_t y = 0; y < ctx->height; y++) {
        uint8_t* src = ctx->working_buf.data + y * rowbytes;

        if (ctx->colorspace == IBOOTIM_COLORSPACE_ARGB) {
            for (uint16_t x = 0; x < ctx->width; x++) {
                row_buf[x*4+0] = src[x*4+2];
                row_buf[x*4+1] = src[x*4+1];
                row_buf[x*4+2] = src[x*4+0];
                row_buf[x*4+3] = 255 - src[x*4+3];
            }
        } else {
            for (uint16_t x = 0; x < ctx->width; x++) {
                row_buf[x*2+0] = src[x*2+0];
                row_buf[x*2+1] = 255 - src[x*2+1];
            }
        }

        png_write_row(write_struct, row_buf);
    }

    free(row_buf);
    row_buf = NULL;

    png_write_end(write_struct, NULL);
    png_destroy_write_struct(&write_struct, &info_struct);

    uint8_t* trimmed = realloc(write_buf.data, write_buf.size);
    *out_buf  = trimmed ? trimmed : write_buf.data;
    *out_size = write_buf.size;
    return IBOOTIM_E_SUCCESS;
}

static ibootim_error_t ibootim_serialize_ibootim(ibootim_ctx_t ctx, uint8_t** out_buf, size_t* out_size) {
    uint8_t pixel_size = ibootim_get_pixel_size_for_color_space(ctx->colorspace);
    if (pixel_size == 0) return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;

    size_t expected_pixel_data_size = (size_t)ctx->width * ctx->height * pixel_size;
    if (ctx->working_buf.size != expected_pixel_data_size) return IBOOTIM_E_MULTIPLE_SUB_IMAGES;

    uint32_t pixel_data_size = (uint32_t)ctx->working_buf.size;
    uint8_t* compressed = malloc(pixel_data_size);
    if (compressed == NULL) return IBOOTIM_E_NO_MEM;

    uint8_t* compressed_end = compress_lzss(compressed, pixel_data_size, ctx->working_buf.data, pixel_data_size);
    if (compressed_end == NULL) {
        free(compressed);
        return IBOOTIM_E_LZSS_XPRESSION_ERROR;
    }
    uint32_t compressed_size = (uint32_t)(compressed_end - compressed);

    size_t total_size = sizeof(ibootim_header_t) + compressed_size;
    uint8_t* buf = calloc(1, total_size);
    if (buf == NULL) {
        free(compressed);
        return IBOOTIM_E_NO_MEM;
    }

    memcpy(buf + sizeof(ibootim_header_t), compressed, compressed_size);
    free(compressed);

    ibootim_header_t* header = (ibootim_header_t*)buf;
    memcpy(header->signature, IBOOTIM_MAGIC_IDENT, 8);
    header->compression_type = IBOOTIM_COMPRESSION_LZSS_IDENT;
    header->colorspace       = ctx->colorspace;
    header->width            = ctx->width;
    header->height           = ctx->height;
    header->x_offset         = ctx->x_offset;
    header->y_offset         = ctx->y_offset;
    header->compressed_size  = compressed_size;

    unsigned header_adler = adler32(1, (unsigned char*)&header->compression_type, (unsigned int)(sizeof(ibootim_header_t) - offsetof(ibootim_header_t, compression_type)));
    header->adler = adler32(header_adler, buf + sizeof(ibootim_header_t), compressed_size);

    *out_buf  = buf;
    *out_size = total_size;
    return IBOOTIM_E_SUCCESS;
}

ibootim_error_t ibootim_write_to_buffer(ibootim_ctx_t ctx, uint8_t** buffer, size_t* size) {
    if (ctx == NULL)                    return IBOOTIM_E_CTX_INVALID;
    if (buffer == NULL || size == NULL) return IBOOTIM_E_NULL_INPUT_BUFFER;

    switch (ctx->type) {
        case IBOOTIM_TYPE_PNG:
            return ibootim_serialize_png(ctx, buffer, size);
        case IBOOTIM_TYPE_LEGACY:
        case IBOOTIM_TYPE_MODERN:
            return ibootim_serialize_ibootim(ctx, buffer, size);
        default:
            return IBOOTIM_E_UNKNOWN_IMAGE_TYPE;
    }
}

ibootim_error_t ibootim_write_to_file(ibootim_ctx_t ctx, const char* filename) {
    if (ctx == NULL)      return IBOOTIM_E_CTX_INVALID;
    if (filename == NULL) return IBOOTIM_E_INVALID_FILENAME;

    uint8_t* buf  = NULL;
    size_t size   = 0;

    ibootim_error_t err = ibootim_write_to_buffer(ctx, &buf, &size);
    if (err != IBOOTIM_E_SUCCESS) return err;

    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        free(buf);
        return IBOOTIM_E_FILE_OPEN_FAILED;
    }

    bool successful_write = (fwrite(buf, 1, size, fp) == size);
    fclose(fp);
    free(buf);

    return successful_write ? IBOOTIM_E_SUCCESS : IBOOTIM_E_FILE_WRITE_FAILED;
}
