/*
 * INF560
 *
 * Image Filtering Project
 */
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "gif_lib.h"

/* Represent one pixel from the image */
typedef struct pixel {
    int r; /* Red */
    int g; /* Green */
    int b; /* Blue */
} pixel;

/* Represent one GIF image (animated or not */
typedef struct animated_gif {
    int n_images; /* Number of images */
    int* width; /* Width of each image */
    int* height; /* Height of each image */
    pixel** p; /* Pixels of each image */
    GifFileType* g; /* Internal representation.
                         DO NOT MODIFY */
} animated_gif;

/*
 * Load a GIF image from a file and return a
 * structure of type animated_gif.
 */
animated_gif*
load_pixels(char* filename)
{
    GifFileType* g;
    ColorMapObject* colmap;
    int error;
    int n_images;
    int* width;
    int* height;
    pixel** p;
    int i;
    animated_gif* image;

    /* Open the GIF image (read mode) */
    g = DGifOpenFileName(filename, &error);
    if (g == NULL) {
        fprintf(stderr, "Error DGifOpenFileName %s\n", filename);
        return NULL;
    }

    /* Read the GIF image */
    error = DGifSlurp(g);
    if (error != GIF_OK) {
        fprintf(stderr,
            "Error DGifSlurp: %d <%s>\n", error, GifErrorString(g->Error));
        return NULL;
    }

    /* Grab the number of images and the size of each image */
    n_images = g->ImageCount;

    width = (int*)malloc(n_images * sizeof(int));
    if (width == NULL) {
        fprintf(stderr, "Unable to allocate width of size %d\n",
            n_images);
        return 0;
    }

    height = (int*)malloc(n_images * sizeof(int));
    if (height == NULL) {
        fprintf(stderr, "Unable to allocate height of size %d\n",
            n_images);
        return 0;
    }

    /* Fill the width and height */
    for (i = 0; i < n_images; i++) {
        width[i] = g->SavedImages[i].ImageDesc.Width;
        height[i] = g->SavedImages[i].ImageDesc.Height;
    }

    /* Get the global colormap */
    colmap = g->SColorMap;
    if (colmap == NULL) {
        fprintf(stderr, "Error global colormap is NULL\n");
        return NULL;
    }

    /* Allocate the array of pixels to be returned */
    p = (pixel**)malloc(n_images * sizeof(pixel*));
    if (p == NULL) {
        fprintf(stderr, "Unable to allocate array of %d images\n",
            n_images);
        return NULL;
    }

    for (i = 0; i < n_images; i++) {
        p[i] = (pixel*)malloc(width[i] * height[i] * sizeof(pixel));
        if (p[i] == NULL) {
            fprintf(stderr, "Unable to allocate %d-th array of %d pixels\n",
                i, width[i] * height[i]);
            return NULL;
        }
    }

    /* Fill pixels */

    /* For each image */
    for (i = 0; i < n_images; i++) {
        int j;

        /* Get the local colormap if needed */
        if (g->SavedImages[i].ImageDesc.ColorMap) {

            /* TODO No support for local color map */
            fprintf(stderr, "Error: application does not support local colormap\n");
            return NULL;

            colmap = g->SavedImages[i].ImageDesc.ColorMap;
        }

        /* Traverse the image and fill pixels */
        for (j = 0; j < width[i] * height[i]; j++) {
            int c;

            c = g->SavedImages[i].RasterBits[j];

            p[i][j].r = colmap->Colors[c].Red;
            p[i][j].g = colmap->Colors[c].Green;
            p[i][j].b = colmap->Colors[c].Blue;
        }
    }

    /* Allocate image info */
    image = (animated_gif*)malloc(sizeof(animated_gif));
    if (image == NULL) {
        fprintf(stderr, "Unable to allocate memory for animated_gif\n");
        return NULL;
    }

    /* Fill image fields */
    image->n_images = n_images;
    image->width = width;
    image->height = height;
    image->p = p;
    image->g = g;

    return image;
}

int output_modified_read_gif(char* filename, GifFileType* g)
{
    GifFileType* g2;
    int error2;

    g2 = EGifOpenFileName(filename, false, &error2);
    if (g2 == NULL) {
        fprintf(stderr, "Error EGifOpenFileName %s\n",
            filename);
        return 0;
    }

    g2->SWidth = g->SWidth;
    g2->SHeight = g->SHeight;
    g2->SColorResolution = g->SColorResolution;
    g2->SBackGroundColor = g->SBackGroundColor;
    g2->AspectByte = g->AspectByte;
    g2->SColorMap = g->SColorMap;
    g2->ImageCount = g->ImageCount;
    g2->SavedImages = g->SavedImages;
    g2->ExtensionBlockCount = g->ExtensionBlockCount;
    g2->ExtensionBlocks = g->ExtensionBlocks;

    error2 = EGifSpew(g2);
    if (error2 != GIF_OK) {
        fprintf(stderr, "Error after writing g2: %d <%s>\n",
            error2, GifErrorString(g2->Error));
        return 0;
    }

    return 1;
}

int store_pixels(char* filename, animated_gif* image)
{
    int n_colors = 0;
    pixel** p;
    int i, j, k;
    GifColorType* colormap;

    /* Initialize the new set of colors */
    colormap = (GifColorType*)malloc(256 * sizeof(GifColorType));
    if (colormap == NULL) {
        fprintf(stderr,
            "Unable to allocate 256 colors\n");
        return 0;
    }

    /* Everything is white by default */
    for (i = 0; i < 256; i++) {
        colormap[i].Red = 255;
        colormap[i].Green = 255;
        colormap[i].Blue = 255;
    }

    /* Change the background color and store it */
    int moy;
    moy = (image->g->SColorMap->Colors[image->g->SBackGroundColor].Red
              + image->g->SColorMap->Colors[image->g->SBackGroundColor].Green
              + image->g->SColorMap->Colors[image->g->SBackGroundColor].Blue)
        / 3;
    if (moy < 0)
        moy = 0;
    if (moy > 255)
        moy = 255;

    colormap[0].Red = moy;
    colormap[0].Green = moy;
    colormap[0].Blue = moy;

    image->g->SBackGroundColor = 0;

    n_colors++;

    /* Process extension blocks in main structure */
    for (j = 0; j < image->g->ExtensionBlockCount; j++) {
        int f;

        f = image->g->ExtensionBlocks[j].Function;
        if (f == GRAPHICS_EXT_FUNC_CODE) {
            int tr_color = image->g->ExtensionBlocks[j].Bytes[3];

            if (tr_color >= 0 && tr_color < 255) {

                int found = -1;

                moy = (image->g->SColorMap->Colors[tr_color].Red
                          + image->g->SColorMap->Colors[tr_color].Green
                          + image->g->SColorMap->Colors[tr_color].Blue)
                    / 3;
                if (moy < 0)
                    moy = 0;
                if (moy > 255)
                    moy = 255;

                for (k = 0; k < n_colors; k++) {
                    if (
                        moy == colormap[k].Red
                        && moy == colormap[k].Green
                        && moy == colormap[k].Blue) {
                        found = k;
                    }
                }
                if (found == -1) {
                    if (n_colors >= 256) {
                        fprintf(stderr,
                            "Error: Found too many colors inside the image\n");
                        return 0;
                    }

                    colormap[n_colors].Red = moy;
                    colormap[n_colors].Green = moy;
                    colormap[n_colors].Blue = moy;

                    image->g->ExtensionBlocks[j].Bytes[3] = n_colors;

                    n_colors++;
                } else {
                    image->g->ExtensionBlocks[j].Bytes[3] = found;
                }
            }
        }
    }

    for (i = 0; i < image->n_images; i++) {
        for (j = 0; j < image->g->SavedImages[i].ExtensionBlockCount; j++) {
            int f;

            f = image->g->SavedImages[i].ExtensionBlocks[j].Function;
            if (f == GRAPHICS_EXT_FUNC_CODE) {
                int tr_color = image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3];

                if (tr_color >= 0 && tr_color < 255) {

                    int found = -1;

                    moy = (image->g->SColorMap->Colors[tr_color].Red
                              + image->g->SColorMap->Colors[tr_color].Green
                              + image->g->SColorMap->Colors[tr_color].Blue)
                        / 3;
                    if (moy < 0)
                        moy = 0;
                    if (moy > 255)
                        moy = 255;

                    for (k = 0; k < n_colors; k++) {
                        if (
                            moy == colormap[k].Red
                            && moy == colormap[k].Green
                            && moy == colormap[k].Blue) {
                            found = k;
                        }
                    }
                    if (found == -1) {
                        if (n_colors >= 256) {
                            fprintf(stderr,
                                "Error: Found too many colors inside the image\n");
                            return 0;
                        }

                        colormap[n_colors].Red = moy;
                        colormap[n_colors].Green = moy;
                        colormap[n_colors].Blue = moy;

                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = n_colors;

                        n_colors++;
                    } else {
                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = found;
                    }
                }
            }
        }
    }

    p = image->p;
    /* Find the number of colors inside the image */
    for (i = 0; i < image->n_images; i++) {

        for (j = 0; j < image->width[i] * image->height[i]; j++) {
            int found = 0;
            for (k = 0; k < n_colors; k++) {
                if (p[i][j].r == colormap[k].Red && p[i][j].g == colormap[k].Green && p[i][j].b == colormap[k].Blue) {
                    found = 1;
                }
            }

            if (found == 0) {
                if (n_colors >= 256) {
                    fprintf(stderr,
                        "Error: Found too many colors inside the image\n");
                    return 0;
                }

                colormap[n_colors].Red = p[i][j].r;
                colormap[n_colors].Green = p[i][j].g;
                colormap[n_colors].Blue = p[i][j].b;
                n_colors++;
            }
        }
    }

    /* Round up to a power of 2 */
    if (n_colors != (1 << GifBitSize(n_colors))) {
        n_colors = (1 << GifBitSize(n_colors));
    }

    /* Change the color map inside the animated gif */
    ColorMapObject* cmo;

    cmo = GifMakeMapObject(n_colors, colormap);
    if (cmo == NULL) {
        fprintf(stderr, "Error while creating a ColorMapObject w/ %d color(s)\n",
            n_colors);
        return 0;
    }

    image->g->SColorMap = cmo;

    /* Update the raster bits according to color map */
    for (i = 0; i < image->n_images; i++) {
        for (j = 0; j < image->width[i] * image->height[i]; j++) {
            int found_index = -1;
            for (k = 0; k < n_colors; k++) {
                if (p[i][j].r == image->g->SColorMap->Colors[k].Red && p[i][j].g == image->g->SColorMap->Colors[k].Green && p[i][j].b == image->g->SColorMap->Colors[k].Blue) {
                    found_index = k;
                }
            }

            if (found_index == -1) {
                fprintf(stderr,
                    "Error: Unable to find a pixel in the color map\n");
                return 0;
            }

            image->g->SavedImages[i].RasterBits[j] = found_index;
        }
    }
    /* Write the final image */
    if (!output_modified_read_gif(filename, image->g)) {
        return 0;
    }

    return 1;
}

void apply_gray_filter(animated_gif* image)
{
    int i, j;
    pixel** p;

    p = image->p;

    for (i = 0; i < image->n_images; i++) {
        for (j = 0; j < image->width[i] * image->height[i]; j++) {
            int moy;

            moy = (p[i][j].r + p[i][j].g + p[i][j].b) / 3;
            if (moy < 0)
                moy = 0;
            if (moy > 255)
                moy = 255;

            p[i][j].r = moy;
            p[i][j].g = moy;
            p[i][j].b = moy;
        }
    }
}

#define CONV(l, c, nb_c) \
    (l) * (nb_c) + (c)
void apply_blur_filter_matrix(int*** image, int size, int threshold, int n_images, int width, int height)
{
    int i, j, k;
    int n_iter = 0;
    int* new_image;

    for (i = 0; i < n_images; i++) {
        n_iter = 0;
        new_image = (int*)malloc(width * height * sizeof(int));

        do {
            int end = 1;
            n_iter++;

            for (j = 0; j < height; j++) {
                for (k = 0; k < size; k++) {
                    new_image[CONV(j, k, width)] = image[i][k][j];
                    new_image[CONV(j, width - 1 - k, width)] = image[i][width - 1 - k][j];
                }
            }

            for (j = size; j < height - size; j++) {
                for (k = size; k < width - size; k++) {
                    int stencil_j, stencil_k;
                    int t = 0;

                    for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                        for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                            t += image[i][k + stencil_k][j + stencil_j];
                        }
                    }

                    new_image[CONV(j, k, width)] = t / ((2 * size + 1) * (2 * size + 1));
                }
            }

            for (j = 0; j < size; j++) {
                for (k = 0; k < width; k++) {
                    new_image[CONV(j, k, width)] = image[i][k][j];
                    new_image[CONV(height - 1 - j, k, width)] = image[i][k][height - 1 - j];
                }
            }

            for (j = 1; j < height - 1; j++) {
                for (k = 1; k < width - 1; k++) {
                    float diff = new_image[CONV(j, k, width)] - image[i][k][j];
                    if (diff > threshold || -diff > threshold) {
                        end = 0;
                    }
                    image[i][k][j] = new_image[CONV(j, k, width)];
                }
            }

        } while (threshold > 0 && !end);

        free(new_image);
    }
}


void apply_blur_filter(animated_gif* image, int size, int threshold)
{
    int i, j, k;
    int width, height;
    int end = 0;
    int n_iter = 0;

    pixel** p;
    pixel* new;

    /* Get the pixels of all images */
    p = image->p;

    /* Process all images */
    for (i = 0; i < image->n_images; i++) {
        n_iter = 0;
        width = image->width[i];
        height = image->height[i];

        /* Allocate array of new pixels */
        new = (pixel*)malloc(width * height * sizeof(pixel));

        /* Perform at least one blur iteration */
        do {
            end = 1;
            n_iter++;

            for (j = 0; j < height - 1; j++) {
                for (k = 0; k < width - 1; k++) {
                    new[CONV(j, k, width)].r = p[i][CONV(j, k, width)].r;
                    new[CONV(j, k, width)].g = p[i][CONV(j, k, width)].g;
                    new[CONV(j, k, width)].b = p[i][CONV(j, k, width)].b;
                }
            }

            /* Apply blur on top part of image (10%) */
            for (j = size; j < height / 10 - size; j++) {
                for (k = size; k < width - size; k++) {
                    int stencil_j, stencil_k;
                    int t = 0;
                    int t_g = 0;
                    int t_b = 0;

                    for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                        for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                            t += p[i][CONV(j + stencil_j, k + stencil_k, width)].r;
                            t_g += p[i][CONV(j + stencil_j, k + stencil_k, width)].g;
                            t_b += p[i][CONV(j + stencil_j, k + stencil_k, width)].b;
                        }
                    }

                    new[CONV(j, k, width)].r = t / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
                }
            }

            /* Copy the middle part of the image */
            for (j = height / 10 - size; j < height * 0.9 + size; j++) {
                for (k = size; k < width - size; k++) {
                    new[CONV(j, k, width)].r = p[i][CONV(j, k, width)].r;
                    new[CONV(j, k, width)].g = p[i][CONV(j, k, width)].g;
                    new[CONV(j, k, width)].b = p[i][CONV(j, k, width)].b;
                }
            }

            /* Apply blur on the bottom part of the image (10%) */
            for (j = height * 0.9 + size; j < height - size; j++) {
                for (k = size; k < width - size; k++) {
                    int stencil_j, stencil_k;
                    int t = 0;
                    int t_g = 0;
                    int t_b = 0;

                    for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                        for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                            t += p[i][CONV(j + stencil_j, k + stencil_k, width)].r;
                            t_g += p[i][CONV(j + stencil_j, k + stencil_k, width)].g;
                            t_b += p[i][CONV(j + stencil_j, k + stencil_k, width)].b;
                        }
                    }

                    new[CONV(j, k, width)].r = t / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
                }
            }

            for (j = 1; j < height - 1; j++) {
                for (k = 1; k < width - 1; k++) {

                    float diff;
                    float diff_g;
                    float diff_b;

                    diff = (new[CONV(j, k, width)].r - p[i][CONV(j, k, width)].r);
                    diff_g = (new[CONV(j, k, width)].g - p[i][CONV(j, k, width)].g);
                    diff_b = (new[CONV(j, k, width)].b - p[i][CONV(j, k, width)].b);

                    if (diff > threshold || -diff > threshold
                        || diff_g > threshold || -diff_g > threshold
                        || diff_b > threshold || -diff_b > threshold) {
                        end = 0;
                    }

                    p[i][CONV(j, k, width)].r = new[CONV(j, k, width)].r;
                    p[i][CONV(j, k, width)].g = new[CONV(j, k, width)].g;
                    p[i][CONV(j, k, width)].b = new[CONV(j, k, width)].b;
                }
            }

        } while (threshold > 0 && !end);
        free(new);
    }
}


void apply_sobel_filter_matrix(int*** image, int width, int height, int n_images)
{
    int i, j, k;

    for (i = 0; i < n_images; i++) {

        int* sobel = (int*)malloc(width * height * sizeof(int));

        for (j = 1; j < height - 1; j++) {
            for (k = 1; k < width - 1; k++) {
                int pixel_no, pixel_n, pixel_ne;
                int pixel_so, pixel_s, pixel_se;
                int pixel_o, pixel, pixel_e;

                float deltaX;
                float deltaY;
                float val;

                pixel_no = image[i][j - 1][k - 1];
                pixel_n = image[i][j - 1][k];
                pixel_ne = image[i][j - 1][k + 1];
                pixel_so = image[i][j + 1][k - 1];
                pixel_s = image[i][j + 1][k];
                pixel_se = image[i][j + 1][k + 1];
                pixel_o = image[i][j][k - 1];
                pixel = image[i][j][k];
                pixel_e = image[i][j][k + 1];

                deltaX = -pixel_no + pixel_ne - 2 * pixel_o + 2 * pixel_e - pixel_so + pixel_se;
                deltaY = pixel_se + 2 * pixel_s + pixel_so - pixel_ne - 2 * pixel_n - pixel_no;

                val = sqrt(deltaX * deltaX + deltaY * deltaY) / 4;

                if (val > 50) {
                    sobel[CONV(j, k, width)] = 255;
                } else {
                    sobel[CONV(j, k, width)] = 0;
                }
            }
        }

        for (j = 1; j < height - 1; j++) {
            for (k = 1; k < width - 1; k++) {
                image[i][k][j] = sobel[CONV(j, k, width)];
            }
        }
        free(sobel);
    }
}



void apply_sobel_filter(animated_gif* image)
{
    int i, j, k;
    int width, height;

    pixel** p;

    p = image->p;

    for (i = 0; i < image->n_images; i++) {
        width = image->width[i];
        height = image->height[i];

        pixel* sobel;

        sobel = (pixel*)malloc(width * height * sizeof(pixel));

        for (j = 1; j < height - 1; j++) {
            for (k = 1; k < width - 1; k++) {
                int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
                int pixel_blue_so, pixel_blue_s, pixel_blue_se;
                int pixel_blue_o, pixel_blue, pixel_blue_e;

                float deltaX_blue;
                float deltaY_blue;
                float val_blue;

                pixel_blue_no = p[i][CONV(j - 1, k - 1, width)].b;
                pixel_blue_n = p[i][CONV(j - 1, k, width)].b;
                pixel_blue_ne = p[i][CONV(j - 1, k + 1, width)].b;
                pixel_blue_so = p[i][CONV(j + 1, k - 1, width)].b;
                pixel_blue_s = p[i][CONV(j + 1, k, width)].b;
                pixel_blue_se = p[i][CONV(j + 1, k + 1, width)].b;
                pixel_blue_o = p[i][CONV(j, k - 1, width)].b;
                pixel_blue = p[i][CONV(j, k, width)].b;
                pixel_blue_e = p[i][CONV(j, k + 1, width)].b;

                deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2 * pixel_blue_o + 2 * pixel_blue_e - pixel_blue_so + pixel_blue_se;

                deltaY_blue = pixel_blue_se + 2 * pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2 * pixel_blue_n - pixel_blue_no;

                val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue) / 4;

                if (val_blue > 50) {
                    sobel[CONV(j, k, width)].r = 255;
                    sobel[CONV(j, k, width)].g = 255;
                    sobel[CONV(j, k, width)].b = 255;
                } else {
                    sobel[CONV(j, k, width)].r = 0;
                    sobel[CONV(j, k, width)].g = 0;
                    sobel[CONV(j, k, width)].b = 0;
                }
            }
        }

        for (j = 1; j < height - 1; j++) {
            for (k = 1; k < width - 1; k++) {
                p[i][CONV(j, k, width)].r = sobel[CONV(j, k, width)].r;
                p[i][CONV(j, k, width)].g = sobel[CONV(j, k, width)].g;
                p[i][CONV(j, k, width)].b = sobel[CONV(j, k, width)].b;
            }
        }
        free(sobel);
    }
}

void deserialize(int* serialized_images, animated_gif* image, int num_images)
{
    int width = serialized_images[1];
    int height = serialized_images[2];
    image->n_images = num_images;

    image->width = malloc(sizeof(int) * num_images);
    image->height = malloc(sizeof(int) * num_images);
    image->p = malloc(sizeof(pixel*) * num_images);

    for (int i = 0; i < num_images; i++) {
        image->width[i] = serialized_images[i * (3 + 3 * width * height) + 1];
        image->height[i] = serialized_images[i * (3 + 3 * width * height) + 2];
        image->p[i] = malloc(sizeof(pixel) * image->width[i] * image->height[i]);
        for (int j = 0; j < image->width[i] * image->height[i]; j++) {
            image->p[i][j].r = serialized_images[i * (3 + 3 * width * height) + j * 3 + 3];
            image->p[i][j].g = serialized_images[i * (3 + 3 * width * height) + j * 3 + 4];
            image->p[i][j].b = serialized_images[i * (3 + 3 * width * height) + j * 3 + 5];
        }
    }
}

int* get_image_from_serialized(int* serialized_images, int index)
{
    int width = serialized_images[1];
    int height = serialized_images[2];
    int initial_index = index * (width * height * 3 + 3);
    int return_value[width * height * 3 + 3];
    return_value[0] = serialized_images[initial_index];
    return_value[1] = serialized_images[initial_index + 1];
    return_value[2] = serialized_images[initial_index + 2];
    for (int j = initial_index + 3; j < (index + 1) * (width * height * 3 + 3); j++) {
        return_value[j - initial_index] = serialized_images[j];
    }
    return return_value;
}

void serialize(animated_gif* image, int** serialized_images, int num_images)
{
    int width = image->width[0];
    int height = image->height[0];
    *serialized_images = malloc(sizeof(int) * num_images * (3 + 3 * width * height));
    for (int i = 0; i < num_images; i++) {
        (*serialized_images)[i * width * height * 3 + 0] = num_images;
        (*serialized_images)[i * width * height * 3 + 1] = width;
        (*serialized_images)[i * width * height * 3 + 2] = height;
        for (int j = 0; j < width * height * 3; j += 3) {
            (*serialized_images)[i * (width * height * 3 + 3) + j + 3] = image->p[i][j / 3].r;
            (*serialized_images)[i * (width * height * 3 + 3) + j + 4] = image->p[i][j / 3].g;
            (*serialized_images)[i * (width * height * 3 + 3) + j + 5] = image->p[i][j / 3].b;
        }
    }
}

animated_gif create_dummy_image()
{
    animated_gif dummy;

    // Set the number of images
    dummy.n_images = 1;

    // Allocate memory for width and height arrays
    dummy.width = malloc(sizeof(int) * dummy.n_images);
    dummy.height = malloc(sizeof(int) * dummy.n_images);

    // Set the width and height for the first image
    dummy.width[0] = 3;
    dummy.height[0] = 3;

    // Allocate memory for pixel array
    dummy.p = malloc(sizeof(pixel*) * dummy.n_images);
    dummy.p[0] = malloc(sizeof(pixel) * dummy.width[0] * dummy.height[0]);

    // Set pixel values for the dummy image
    int pixelValues[3][3][3] = {
        { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 } },
        { { 255, 255, 0 }, { 255, 0, 255 }, { 0, 255, 255 } },
        { { 128, 128, 128 }, { 0, 0, 0 }, { 255, 255, 255 } }
    };

    for (int i = 0; i < dummy.width[0] * dummy.height[0]; i++) {
        dummy.p[0][i].r = pixelValues[i / dummy.width[0]][i % dummy.width[0]][0];
        dummy.p[0][i].g = pixelValues[i / dummy.width[0]][i % dummy.width[0]][1];
        dummy.p[0][i].b = pixelValues[i / dummy.width[0]][i % dummy.width[0]][2];
    }

    return dummy;
}

bool is_dummy_image(const animated_gif* image)
{
    if (image->n_images != 1) {
        return false;
    }

    if (image->width[0] != 3 || image->height[0] != 3) {
        return false;
    }

    int expectedPixelValues[3][3][3] = {
        { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 } },
        { { 255, 255, 0 }, { 255, 0, 255 }, { 0, 255, 255 } },
        { { 128, 128, 128 }, { 0, 0, 0 }, { 255, 255, 255 } }
    };

    for (int i = 0; i < 3 * 3; i++) {
        if (image->p[0][i].r != expectedPixelValues[i / 3][i % 3][0] || image->p[0][i].g != expectedPixelValues[i / 3][i % 3][1] || image->p[0][i].b != expectedPixelValues[i / 3][i % 3][2]) {
            return false;
        }
    }

    return true;
}

int*** gif_to_matrix(const animated_gif* image, int num_images)
{
    // create a 3D matrix [num_images, width, height] representing the gif
    int width = image->width[0];
    int height = image->height[0];

    int*** serialized_images = (int***)malloc(num_images * sizeof(int**));
    for (int i = 0; i < num_images; i++) {
        serialized_images[i] = (int**)malloc(width * sizeof(int*));
        for (int j = 0; j < width; j++) {
            serialized_images[i][j] = (int*)malloc(height * sizeof(int));
        }
    }

    for (int i = 0; i < num_images; i++) {
        for (int j = 0; j < width * height; j++) {
            int moy;
            pixel** p = image->p;
            moy = (p[i][j].r + p[i][j].g + p[i][j].b) / 3;
            if (moy < 0)
                moy = 0;
            if (moy > 255)
                moy = 255;
            serialized_images[i][j % width][j / width] = moy;
        }
    }

    return serialized_images;
}

void matrix_to_gif(animated_gif* image, int*** gif_matrix) {
    int width = image->width[0];
    int height = image->height[0];

    for (int i = 0; i < image->n_images; i++) {
        for (int j = 0; j < width; j++) {
            for(int k = 0; k < height; k++) {
                image->p[i][CONV(j, k, width)].r = gif_matrix[i][j][k];
                image->p[i][CONV(j, k, width)].g = gif_matrix[i][j][k];
                image->p[i][CONV(j, k, width)].b = gif_matrix[i][j][k];
            }
        }
    }
}

/*
 * Main entry point
 */
int main(int argc, char** argv)
{
    char* input_filename;
    char* output_filename;
    animated_gif* image = NULL;

    int num_images;
    int* image_information = malloc(sizeof(int) * 3);
    struct timeval t1, t2;
    double duration;
    int root_process = 0;
    int rank, size;
    FILE* fptr;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Check command-line arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.gif output.gif \n", argv[0]);
        return 1;
    }
    input_filename = argv[1];
    output_filename = argv[2];

    if (rank == root_process) {
        /* IMPORT Timer start */
        gettimeofday(&t1, NULL);
        /* Load file and store the pixels in array */
        image = load_pixels(input_filename);
        if (image == NULL) {
            return 1;
        }
        /* IMPORT Timer stop */
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
        printf("GIF loaded from file %s with %d image(s) in %lf s\n",
            input_filename, image->n_images, duration);
        image_information[0] = image->n_images;
        image_information[1] = image->width[0];
        image_information[2] = image->height[0];
        int*** gif_matrix = gif_to_matrix(image, image->n_images);
    }
    int*** current_gif;
    MPI_Bcast(image_information, 3, MPI_INT, root_process, MPI_COMM_WORLD);
    int width = image_information[1];
    int height = image_information[2];
    int n_images = image_information[0];
    int images_per_rank = (n_images / (size - 1)); // only for rank>0
    int remainder_images = n_images % (size - 1);
    int nb_pixels = width * height;
    if (rank == root_process) {
        int count = 0;
        int received_rank;
        for (int i = 1; i < size; i++) {
            int nb_images = images_per_rank + (remainder_images <= i ? 1 : 0);
            MPI_SendRecv(gif_matrix + count * nb_pixels, nb_images * nb_pixels, MPI_INT, i, 0, gif_matrix + count * nb_pixels, count * nb_images, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            count += nb_images;
        }
    } else {
        int nb_images_local = images_per_rank + (remainder_images <= rank ? 1 : 0);
        int*** buffer = (int***)malloc(nb_images_local * sizeof(int**));
        for (int i = 0; i < num_images; i++) {
            buffer[i] = (int**)malloc(width * sizeof(int*));
            for (int j = 0; j < width; j++) {
                buffer[i][j] = (int*)malloc(height * sizeof(int));
            }
        }
        MPI_Recv(&(buffer[0][0][0]), nb_images_local * nb_pixels, MPI_INT, root_process, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        apply_blur_filter_matrix(buffer, 5, 20, nb_images_local, width, height);
        apply_sobel_filter_matrix(buffer, width, height, nb_images_local);
        MPI_Send(&(buffer[0][0][0]), nb_images_local * nb_pixels, MPI_INT, root_process, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


        for (int i = 0; i < nb_images_local; i++) {
            for (int j = 0; j < width; j++) {
                free(buffer[i][j]);
            }
            free(buffer[i]);
        }
        free(buffer);
    }

    if (rank == root_process) {
        // MPI_Datatype subtype[size-1];
        // images_per_rank = image_information%size;
        // int subarray_size[3] = {images_per_rank, image_information[1], image_information[2]};
        // int starting_image = images_per_rank*(rank-1);
        // int subarray_start[3] = {starting_image, 0, 0};
        // MPI_Type_create_subarrray(3, array_size, subarray_size, subarray_start, MPI_ORDER_C, MPI_INT, &subtype);
        // MPI_Type_commit(&subtype);
        // MPI_Datatype subtype;
    }
    gettimeofday(&t1, NULL);

    // /* Apply blur filter with convergence value */
    // apply_blur_filter(image, 5, 20);

    // /* Apply sobel filter on pixels */
    // apply_sobel_filter(image);

    if (rank != root_process) {
        MPI_Finalize();
        return 0;
    }

    matrix_to_gif(image, gif_matrix);
    /* FILTER Timer stop */
    gettimeofday(&t2, NULL);

    duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);

    printf("SOBEL done in %lf s\n", duration);

    fptr = fopen("perf.log", "a");
    fprintf(fptr, "%d %d %f s\n", image->width[0], image->height[0], duration);
    fclose(fptr);

    /* EXPORT Timer start */
    gettimeofday(&t1, NULL);

    /* Store file from array of pixels to GIF file */
    if (!store_pixels(output_filename, image)) {
        return 1;
    }

    /* EXPORT Timer stop */
    gettimeofday(&t2, NULL);

    duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
    free(image_information);
    if (rank == root_process) {
        free(serialized_image);
    }
    printf("Export done in %lf s in file %s\n", duration, output_filename);
    MPI_Finalize();

    return 0;
}
