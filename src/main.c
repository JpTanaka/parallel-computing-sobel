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


// #include "cuda_functions.h"
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
                }
                else {
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
                    }
                    else {
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
void apply_blur_filter_flattened_array(int* image, int size, int threshold, int width, int height)
{
    int i, j, k;
    int end;
    i = 0;

    int* new_image = (int*)malloc(width * height * sizeof(int));
    do {
        end = 1;
        for (j = 0; j < height - 1; j++) {
            for (k = 0; k < width - 1; k++) {
                new_image[CONV(j, k, width)] = image[i * width * height + CONV(j, k, width)];
            }
        }
        /* Apply blur on top part of image (10%) */
        for (j = size; j < height / 10 - size; j++) {
            for (k = size; k < width - size; k++) {
                int stencil_j, stencil_k;
                int t = 0;

                for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                    for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                        t += image[i * width * height + CONV(j + stencil_j, k + stencil_k, width)];
                    }
                }

                new_image[CONV(j, k, width)] = t / ((2 * size + 1) * (2 * size + 1));
            }
        }

        /* Copy the middle part of the image */
        for (j = height / 10 - size; j < height * 0.9 + size; j++) {
            for (k = size; k < width - size; k++) {
                new_image[CONV(j, k, width)] = image[i * width * height + CONV(j, k, width)];
            }
        }

        /* Apply blur on the bottom part of the image (10%) */
        for (j = height * 0.9 + size; j < height - size; j++) {
            for (k = size; k < width - size; k++) {
                int stencil_j, stencil_k;
                int t = 0;

                for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                    for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                        t += image[i * width * height + CONV(j + stencil_j, k + stencil_k, width)];
                    }
                }

                new_image[CONV(j, k, width)] = t / ((2 * size + 1) * (2 * size + 1));
            }
        }

        for (j = 1; j < height - 1; j++) {
            for (k = 1; k < width - 1; k++) {
                float diff = new_image[CONV(j, k, width)] - image[i * width * height + CONV(j, k, width)];
                if (diff > threshold || -diff > threshold) {
                    end = 0;
                }
                image[i * width * height + CONV(j, k, width)] = new_image[CONV(j, k, width)];
            }
        }
    } while (threshold > 0 && !end);

    free(new_image);
}

#define CONV(l, c, nb_c) \
    (l) * (nb_c) + (c)

void apply_blur_filter(animated_gif* image, int size, int threshold)
{
    int i, j, k;
    int width, height;
    int end = 0;

    pixel** p;
    pixel* new;

    /* Get the pixels of all images */
    p = image->p;

    /* Process all images */
    for (i = 0; i < image->n_images; i++) {
        width = image->width[i];
        height = image->height[i];

        /* Allocate array of new pixels */
        new = (pixel*)malloc(width * height * sizeof(pixel));

        /* Perform at least one blur iteration */
        do {
            end = 1;

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

void apply_sobel_filter_flattened_array(int* image, int width, int height)
{
    int i, j, k;

    int* sobel = (int*)malloc(width * height * sizeof(int));
    for (j = 1; j < height - 1; j++) {
        for (k = 1; k < width - 1; k++) {
            int pixel_no, pixel_n, pixel_ne;
            int pixel_so, pixel_s, pixel_se;
            int pixel_o, pixel_e;

            float deltaX;
            float deltaY;
            float val;

            pixel_no = image[CONV(j - 1, k - 1, width)];
            pixel_n = image[CONV(j - 1, k, width)];
            pixel_ne = image[CONV(j - 1, k + 1, width)];
            pixel_so = image[CONV(j + 1, k - 1, width)];
            pixel_s = image[CONV(j + 1, k, width)];
            pixel_se = image[CONV(j + 1, k + 1, width)];
            pixel_o = image[CONV(j, k - 1, width)];
            pixel_e = image[CONV(j, k + 1, width)];

            deltaX = -pixel_no + pixel_ne - 2 * pixel_o + 2 * pixel_e - pixel_so + pixel_se;
            deltaY = pixel_se + 2 * pixel_s + pixel_so - pixel_ne - 2 * pixel_n - pixel_no;

            val = sqrt(deltaX * deltaX + deltaY * deltaY) / 4;

            if (val > 50) {
                sobel[CONV(j, k, width)] = 255;
            }
            else {
                sobel[CONV(j, k, width)] = 0;
            }
        }
    }

    for (j = 1; j < height - 1; j++) {
        for (k = 1; k < width - 1; k++) {
            image[CONV(j, k, width)] = sobel[CONV(j, k, width)];
        }
    }
    free(sobel);
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
                }
                else {
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

int* gif_to_flatten_array(const animated_gif* image, int num_images)
{

    int nb_pixels = 0;
    for(int i = 0; i < image->n_images; i++) {
        nb_pixels += image->width[i]*image->height[i]; 
    }
    int idx=0;
    int* array = malloc(nb_pixels * sizeof(int));
    for (int i = 0; i < num_images; i++) {
        for (int j = 0; j < image->height[i]; j++) {
            for (int k = 0; k < image->width[i]; k++) {
                int moy;
                pixel** p = image->p;
                moy = (p[i][CONV(j, k, image->width[i])].r + p[i][CONV(j, k, image->width[i])].g + p[i][CONV(j, k, image->width[i])].b) / 3;
                if (moy < 0)
                    moy = 0;
                if (moy > 255)
                    moy = 255;
                array[idx++] = moy;
            }
        }
    }

    return array;
}

int get_first_image_of_rank(int rank, int n_images, int size) {
    if (rank == 0) {
        return NULL;
    }
    int images_per_rank = (n_images / (size - 1));
    int remainder_images = n_images % (size - 1);
    int current_image = 0;
    for (int i = 1; i < rank; i++) {
            int nb_images = images_per_rank + (remainder_images >= i ? 1 : 0);
            current_image+=nb_images;
    }
    return current_image;
}

long long int* get_image_offsets(int* widths, int* heights, int n_images) {
    long long int* offsets = malloc((n_images + 1) * sizeof(long long int));
    long long int current_offset = 0;
    for (int i = 0; i < n_images; i++) {
        offsets[i] = current_offset;
        current_offset += (long long int)widths[i] * heights[i];
    }
    offsets[n_images] = current_offset;

    return offsets;
}

void process_images(int* buffer, int n_images, int* widths, int* heights){
    int offset = 0;
    int use_cuda = 1;
    for(int i = 0; i < n_images; i++) {
        if(use_cuda) {
            apply_blur_filter_cuda(buffer+offset, 20, 5, widths[i], heights[i]);
            apply_sobel_filter_cuda(buffer+offset, widths[i], heights[i]);

        }
        else{
            apply_blur_filter_flattened_array(buffer + offset, 5, 20, widths[i], heights[i]);
            apply_sobel_filter_flattened_array(buffer + offset, widths[i], heights[i]);
        }
        offset+= widths[i]*heights[i];
    }
}

int get_number_images_to_rank(int rank, int n_images, int size){
    int images_per_rank = (n_images / (size - 1));
    int remainder_images = n_images % (size - 1);
    return images_per_rank + (remainder_images >= rank ? 1 : 0);
}

void flattened_matrix_to_gif(animated_gif* image, int* flattened_matrix) {
    int idx = 0;
    for (int i = 0; i < image->n_images; i++) {
        for (int j = 0; j < image->height[i]; j++) {
            for (int k = 0; k < image->width[i]; k++) {
                image->p[i][CONV(j, k, image->width[i])].r = flattened_matrix[idx];
                image->p[i][CONV(j, k, image->width[i])].g = flattened_matrix[idx];
                image->p[i][CONV(j, k, image->width[i])].b = flattened_matrix[idx];
                idx++;
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
    int n_images;
    int* widths;
    int* heights;

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

    int* flattened_gif_matrix;
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
        n_images = image->n_images;
        flattened_gif_matrix = gif_to_flatten_array(image, image->n_images);
    }
    MPI_Bcast(&n_images, 1, MPI_INT, root_process, MPI_COMM_WORLD);
    widths = malloc(n_images*sizeof(int));
    heights = malloc(n_images*sizeof(int));
    if(rank==root_process) {
        widths = image->width;
        heights = image->height;
    }
    MPI_Bcast(widths, n_images, MPI_INT, root_process, MPI_COMM_WORLD);
    MPI_Bcast(heights, n_images, MPI_INT, root_process, MPI_COMM_WORLD);
    long long int* offsets = get_image_offsets(widths, heights, n_images);
    gettimeofday(&t1, NULL);
    
    if (rank == root_process) {

        int count = 0;
        for (int i = 1; i < size; i++) {
            int nb_images = get_number_images_to_rank(i, n_images, size);
            if (nb_images == 0) {
                break;
            }
            int current_image = get_first_image_of_rank(i, n_images, size);
            int nb_pixels = offsets[current_image+nb_images]-offsets[current_image];
            MPI_Sendrecv(flattened_gif_matrix + offsets[current_image], nb_pixels, MPI_INT, i, 0,
                flattened_gif_matrix + offsets[current_image], nb_pixels, MPI_INT, i, 0,
                MPI_COMM_WORLD, NULL);
        }
    }
    else {
        int nb_images_local = get_number_images_to_rank(rank, n_images, size);
        if (nb_images_local == 0) {
            free(image_information);
            MPI_Finalize();
            return 0;
        }
        int first_image = get_first_image_of_rank(rank, n_images, size);
        int nb_pixels = offsets[first_image+nb_images_local]-offsets[first_image];
        int* buffer = (int*)malloc(nb_pixels*sizeof(int));
        MPI_Recv(buffer, nb_pixels, MPI_INT, root_process, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        process_images(buffer, nb_images_local, widths+first_image, heights+first_image);
        MPI_Send(buffer, nb_pixels, MPI_INT, root_process, 0, MPI_COMM_WORLD);
        free(buffer);
    }
    free(offsets);


    if (rank != root_process) {
        free(image_information);
        MPI_Finalize();
        return 0;
    }
    flattened_matrix_to_gif(image, flattened_gif_matrix);
    free(flattened_gif_matrix);
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
    printf("Export done in %lf s in file %s\n", duration, output_filename);
    MPI_Finalize();

    return 0;
}
