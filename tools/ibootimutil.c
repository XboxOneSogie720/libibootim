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

#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libibootim.h>

typedef enum {
    FORCE_TYPE_RETAIN,
    FORCE_TYPE_ARGB,
    FORCE_TYPE_GRAYSCALE
} ibootimutil_force_type_t;

#define GETOPT_ARGSTR "hi:o:x:y:cg"

static struct option longopts[] = {
    { "help",            no_argument,       NULL, 'h' },
    { "input",           required_argument, NULL, 'i' },
    { "output",          required_argument, NULL, 'o' },
    { "x-offset",        required_argument, NULL, 'x' },
    { "y-offset",        required_argument, NULL, 'y' },
    { "force-argb",      no_argument,       NULL, 'c' },
    { "force-grayscale", no_argument,       NULL, 'g' },
    { NULL, 0, NULL, 0 }
};

void print_usage(char** argv, bool is_error) {
    fprintf(is_error ? stderr : stdout,
            "Usage: %s [Options]\n"
            "\n"
            "Options:\n"
            "    -h, --help                     Display this help message and exit.\n"
            "    -i, --input <PATH>             Path to your input file (png or ibootim).\n"
            "    -o, --output <PATH>            Path to write the output file to (opposite of input, automatically decided).\n"
            "    -x, --x-offset <signed num>    X-offset to apply to your output image (only applies if conversion is png->ibootim).\n"
            "    -y, --y-offset <signed num>    Y-offset to apply to your output image (only applies if conversion is png->ibootim).\n"
            "    -c, --force-argb               Force colorspace to argb.\n"
            "    -g, --force-grayscale          Force colorspace to grayscale.\n",
            argv[0]
    );
}

void print_image_info(const char* title, ibootim_ctx_t ctx) {
    ibootim_type_t type             = IBOOTIM_TYPE_UNKNOWN;
    ibootim_colorspace_t colorspace = IBOOTIM_COLORSPACE_UNKNOWN;
    uint16_t width                  = 0;
    uint16_t height                 = 0;
    int16_t x_offset                = 0;
    int16_t y_offset                = 0;

    printf(
        "%s:\n"
        "    Type:       %s\n"
        "    Colorspace: %s\n"
        "    Width:      %u\n"
        "    Height:     %u\n"
        "    X-offset:   %d\n"
        "    Y-offset:   %d\n",
        title,
        (ibootim_get_type(ctx, &type)             == IBOOTIM_E_SUCCESS) ? type_to_str(type) : "N/A",
        (ibootim_get_colorspace(ctx, &colorspace) == IBOOTIM_E_SUCCESS) ? colorspace_to_str(colorspace) : "N/A",
        (ibootim_get_width(ctx, &width)           == IBOOTIM_E_SUCCESS) ? width : width,
        (ibootim_get_height(ctx, &height)         == IBOOTIM_E_SUCCESS) ? height : height,
        (ibootim_get_x_offset(ctx, &x_offset)     == IBOOTIM_E_SUCCESS) ? x_offset : x_offset,
        (ibootim_get_y_offset(ctx, &y_offset)     == IBOOTIM_E_SUCCESS) ? y_offset : y_offset
    );
}

int main(int argc, char** argv) {
    int opt                             = 0;
    char* input_filename                = NULL;
    char* output_filename               = NULL;
    int16_t x_offset                    = 0;
    int16_t y_offset                    = 0;
    ibootimutil_force_type_t force_type = FORCE_TYPE_RETAIN;

    while ((opt = getopt_long(argc, argv, GETOPT_ARGSTR, longopts, NULL)) > 0) {
        switch (opt) {
            case 'h':
                print_usage(argv, false);
                free(output_filename);
                free(input_filename);
                return 0;
            
            case 'i':
                free(input_filename);
                input_filename = strdup(optarg);
                if (input_filename == NULL) {
                    fprintf(stderr, "Error: Failed to allocate memory for the input filename.\n");
                    free(output_filename);
                    return -1;
                }
                break;

            case 'o':
                free(output_filename);
                output_filename = strdup(optarg);
                if (output_filename == NULL) {
                    fprintf(stderr, "Error: Failed to allocate memory for the output filename.\n");
                    free(input_filename);
                    return -1;
                }
                break;

            case 'x': {
                int32_t val = (int32_t)strtol(optarg, NULL, 10);
                if (val < INT16_MIN || val > INT16_MAX) {
                    fprintf(stderr, "Error: X-offset value %d is out of range [%d, %d].\n", val, INT16_MIN, INT16_MAX);
                    return -1;
                }
                x_offset = (int16_t)val;
                break;
            }

            case 'y': {
                int32_t val = (int32_t)strtol(optarg, NULL, 10);
                if (val < INT16_MIN || val > INT16_MAX) {
                    fprintf(stderr, "Error: Y-offset value %d is out of range [%d, %d].\n", val, INT16_MIN, INT16_MAX);
                    return -1;
                }
                y_offset = (int16_t)val;
                break;
            }

            case 'c':
                force_type = FORCE_TYPE_ARGB;
                break;

            case 'g':
                force_type = FORCE_TYPE_GRAYSCALE;
                break;
            
            default:
                fprintf(stderr, "Unknown argument: %s\n", optarg);
                print_usage(argv, true);
                free(output_filename);
                free(input_filename);
                return -1;
        }
    }

    // Check
    if (input_filename == NULL) {
        fprintf(stderr, "Error: No input filename provided.\n");
        print_usage(argv, true);
        free(output_filename);
        return -1;
    }
    if (output_filename == NULL) {
        fprintf(stderr, "Error: No output filename provided.\n");
        print_usage(argv, true);
        free(input_filename);
        return -1;
    }

    // Begin
    ibootim_error_t error = IBOOTIM_E_SUCCESS;
    ibootim_ctx_t ctx     = NULL;

    printf("Loading file %s...\n", input_filename);
    error = ibootim_load_from_file(input_filename, &ctx);
    if (error != IBOOTIM_E_SUCCESS) {
        fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
        free(output_filename);
        free(input_filename);
        return -1;
    }

    print_image_info("Input Image Info", ctx);

    ibootim_type_t type = IBOOTIM_TYPE_UNKNOWN;
    error = ibootim_get_type(ctx, &type);
    if (error != IBOOTIM_E_SUCCESS) {
        fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
        goto error_exit;
    }

    if (type == IBOOTIM_TYPE_PNG) {
        printf("Converting to ibootim...\n");

        error = ibootim_set_type(ctx, IBOOTIM_TYPE_MODERN);
        if (error != IBOOTIM_E_SUCCESS) {
            fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
            goto error_exit;
        }

        if (force_type == FORCE_TYPE_ARGB) {
            error = ibootim_set_colorspace(ctx, IBOOTIM_COLORSPACE_ARGB);
            if (error != IBOOTIM_E_SUCCESS) {
                fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
                goto error_exit;
            }
        }

        if (force_type == FORCE_TYPE_GRAYSCALE) {
            error = ibootim_set_colorspace(ctx, IBOOTIM_COLORSPACE_GRAYSCALE);
            if (error != IBOOTIM_E_SUCCESS) {
                fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
                goto error_exit;
            }
        }

        error = ibootim_set_x_offset(ctx, x_offset);
        if (error != IBOOTIM_E_SUCCESS) {
            fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
            goto error_exit;
        }

        error = ibootim_set_y_offset(ctx, y_offset);
        if (error != IBOOTIM_E_SUCCESS) {
            fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
            goto error_exit;
        }
    } else {
        printf("Converting to png...\n");

        error = ibootim_set_type(ctx, IBOOTIM_TYPE_PNG);
        if (error != IBOOTIM_E_SUCCESS) {
            fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
            goto error_exit;
        }

        if (force_type == FORCE_TYPE_ARGB) {
            error = ibootim_set_colorspace(ctx, IBOOTIM_COLORSPACE_ARGB);
            if (error != IBOOTIM_E_SUCCESS) {
                fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
                goto error_exit;
            }
        }

        if (force_type == FORCE_TYPE_GRAYSCALE) {
            error = ibootim_set_colorspace(ctx, IBOOTIM_COLORSPACE_GRAYSCALE);
            if (error != IBOOTIM_E_SUCCESS) {
                fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
                goto error_exit;
            }
        }
    }

    print_image_info("Output Image Info", ctx);

    printf("Writing...\n");
    error = ibootim_write_to_file(ctx, output_filename);
    if (error != IBOOTIM_E_SUCCESS) {
        fprintf(stderr, "Error: %s\n", ibootim_strerror(error));
        goto error_exit;
    }

    printf("Success!\n");

    // Cleanup
    ibootim_free_ctx(&ctx);
    free(output_filename);
    free(input_filename);
    return 0;

error_exit:
    ibootim_free_ctx(&ctx);
    free(output_filename);
    free(input_filename);
    return -1;
}
