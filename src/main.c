/*
 * INF560
 *
 * Image Filtering Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <mpi.h>

#include "gif_lib.h"

 /* Set this macro to 1 to enable debugging information */
#define SOBELF_DEBUG 0

/* Represent one pixel from the image */
typedef struct pixel
{
    int r; /* Red */
    int g; /* Green */
    int b; /* Blue */
} pixel;

/* Represent one GIF image (animated or not */
typedef struct animated_gif
{
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
    if (g == NULL)
    {
        fprintf(stderr, "Error DGifOpenFileName %s\n", filename);
        return NULL;
    }

    /* Read the GIF image */
    error = DGifSlurp(g);
    if (error != GIF_OK)
    {
        fprintf(stderr,
            "Error DGifSlurp: %d <%s>\n", error, GifErrorString(g->Error));
        return NULL;
    }

    /* Grab the number of images and the size of each image */
    n_images = g->ImageCount;

    width = (int*)malloc(n_images * sizeof(int));
    if (width == NULL)
    {
        fprintf(stderr, "Unable to allocate width of size %d\n",
            n_images);
        return 0;
    }

    height = (int*)malloc(n_images * sizeof(int));
    if (height == NULL)
    {
        fprintf(stderr, "Unable to allocate height of size %d\n",
            n_images);
        return 0;
    }

    /* Fill the width and height */
    for (i = 0; i < n_images; i++)
    {
        width[i] = g->SavedImages[i].ImageDesc.Width;
        height[i] = g->SavedImages[i].ImageDesc.Height;
    }


    /* Get the global colormap */
    colmap = g->SColorMap;
    if (colmap == NULL)
    {
        fprintf(stderr, "Error global colormap is NULL\n");
        return NULL;
    }

    /* Allocate the array of pixels to be returned */
    p = (pixel**)malloc(n_images * sizeof(pixel*));
    if (p == NULL)
    {
        fprintf(stderr, "Unable to allocate array of %d images\n",
            n_images);
        return NULL;
    }

    for (i = 0; i < n_images; i++)
    {
        p[i] = (pixel*)malloc(width[i] * height[i] * sizeof(pixel));
        if (p[i] == NULL)
        {
            fprintf(stderr, "Unable to allocate %d-th array of %d pixels\n",
                i, width[i] * height[i]);
            return NULL;
        }
    }

    /* Fill pixels */

    /* For each image */
    for (i = 0; i < n_images; i++)
    {
        int j;

        /* Get the local colormap if needed */
        if (g->SavedImages[i].ImageDesc.ColorMap)
        {

            /* TODO No support for local color map */
            fprintf(stderr, "Error: application does not support local colormap\n");
            return NULL;

            colmap = g->SavedImages[i].ImageDesc.ColorMap;
        }

        /* Traverse the image and fill pixels */
        for (j = 0; j < width[i] * height[i]; j++)
        {
            int c;

            c = g->SavedImages[i].RasterBits[j];

            p[i][j].r = colmap->Colors[c].Red;
            p[i][j].g = colmap->Colors[c].Green;
            p[i][j].b = colmap->Colors[c].Blue;
        }
    }

    /* Allocate image info */
    image = (animated_gif*)malloc(sizeof(animated_gif));
    if (image == NULL)
    {
        fprintf(stderr, "Unable to allocate memory for animated_gif\n");
        return NULL;
    }

    /* Fill image fields */
    image->n_images = n_images;
    image->width = width;
    image->height = height;
    image->p = p;
    image->g = g;

#if SOBELF_DEBUG
    printf("-> GIF w/ %d image(s) with first image of size %d x %d\n",
        image->n_images, image->width[0], image->height[0]);
#endif

    return image;
}

int
output_modified_read_gif(char* filename, GifFileType* g)
{
    GifFileType* g2;
    int error2;

#if SOBELF_DEBUG
    printf("Starting output to file %s\n", filename);
#endif

    g2 = EGifOpenFileName(filename, false, &error2);
    if (g2 == NULL)
    {
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
    if (error2 != GIF_OK)
    {
        fprintf(stderr, "Error after writing g2: %d <%s>\n",
            error2, GifErrorString(g2->Error));
        return 0;
    }

    return 1;
}


int
store_pixels(char* filename, animated_gif* image)
{
    int n_colors = 0;
    pixel** p;
    int i, j, k;
    GifColorType* colormap;

    /* Initialize the new set of colors */
    colormap = (GifColorType*)malloc(256 * sizeof(GifColorType));
    if (colormap == NULL)
    {
        fprintf(stderr,
            "Unable to allocate 256 colors\n");
        return 0;
    }

    /* Everything is white by default */
    for (i = 0; i < 256; i++)
    {
        colormap[i].Red = 255;
        colormap[i].Green = 255;
        colormap[i].Blue = 255;
    }

    /* Change the background color and store it */
    int moy;
    moy = (
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Red
        +
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Green
        +
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Blue
        ) / 3;
    if (moy < 0) moy = 0;
    if (moy > 255) moy = 255;

#if SOBELF_DEBUG
    printf("[DEBUG] Background color (%d,%d,%d) -> (%d,%d,%d)\n",
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Red,
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Green,
        image->g->SColorMap->Colors[image->g->SBackGroundColor].Blue,
        moy, moy, moy);
#endif

    colormap[0].Red = moy;
    colormap[0].Green = moy;
    colormap[0].Blue = moy;

    image->g->SBackGroundColor = 0;

    n_colors++;

    /* Process extension blocks in main structure */
    for (j = 0; j < image->g->ExtensionBlockCount; j++)
    {
        int f;

        f = image->g->ExtensionBlocks[j].Function;
        if (f == GRAPHICS_EXT_FUNC_CODE)
        {
            int tr_color = image->g->ExtensionBlocks[j].Bytes[3];

            if (tr_color >= 0 &&
                tr_color < 255)
            {

                int found = -1;

                moy =
                    (
                        image->g->SColorMap->Colors[tr_color].Red
                        +
                        image->g->SColorMap->Colors[tr_color].Green
                        +
                        image->g->SColorMap->Colors[tr_color].Blue
                        ) / 3;
                if (moy < 0) moy = 0;
                if (moy > 255) moy = 255;

#if SOBELF_DEBUG
                printf("[DEBUG] Transparency color image %d (%d,%d,%d) -> (%d,%d,%d)\n",
                    i,
                    image->g->SColorMap->Colors[tr_color].Red,
                    image->g->SColorMap->Colors[tr_color].Green,
                    image->g->SColorMap->Colors[tr_color].Blue,
                    moy, moy, moy);
#endif

                for (k = 0; k < n_colors; k++)
                {
                    if (
                        moy == colormap[k].Red
                        &&
                        moy == colormap[k].Green
                        &&
                        moy == colormap[k].Blue
                        )
                    {
                        found = k;
                    }
                }
                if (found == -1)
                {
                    if (n_colors >= 256)
                    {
                        fprintf(stderr,
                            "Error: Found too many colors inside the image\n"
                        );
                        return 0;
                    }

#if SOBELF_DEBUG
                    printf("[DEBUG]\tNew color %d\n",
                        n_colors);
#endif

                    colormap[n_colors].Red = moy;
                    colormap[n_colors].Green = moy;
                    colormap[n_colors].Blue = moy;


                    image->g->ExtensionBlocks[j].Bytes[3] = n_colors;

                    n_colors++;
                }
                else
                {
                    image->g->ExtensionBlocks[j].Bytes[3] = found;
                }
            }
        }
    }

    for (i = 0; i < image->n_images; i++)
    {
        for (j = 0; j < image->g->SavedImages[i].ExtensionBlockCount; j++)
        {
            int f;

            f = image->g->SavedImages[i].ExtensionBlocks[j].Function;
            if (f == GRAPHICS_EXT_FUNC_CODE)
            {
                int tr_color = image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3];

                if (tr_color >= 0 &&
                    tr_color < 255)
                {

                    int found = -1;

                    moy =
                        (
                            image->g->SColorMap->Colors[tr_color].Red
                            +
                            image->g->SColorMap->Colors[tr_color].Green
                            +
                            image->g->SColorMap->Colors[tr_color].Blue
                            ) / 3;
                    if (moy < 0) moy = 0;
                    if (moy > 255) moy = 255;

                    for (k = 0; k < n_colors; k++)
                    {
                        if (
                            moy == colormap[k].Red
                            &&
                            moy == colormap[k].Green
                            &&
                            moy == colormap[k].Blue
                            )
                        {
                            found = k;
                        }
                    }
                    if (found == -1)
                    {
                        if (n_colors >= 256)
                        {
                            fprintf(stderr,
                                "Error: Found too many colors inside the image\n"
                            );
                            return 0;
                        }

                        colormap[n_colors].Red = moy;
                        colormap[n_colors].Green = moy;
                        colormap[n_colors].Blue = moy;


                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = n_colors;

                        n_colors++;
                    }
                    else
                    {
                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = found;
                    }
                }
            }
        }
    }

    p = image->p;
    /* Find the number of colors inside the image */
    for (i = 0; i < image->n_images; i++)
    {

        for (j = 0; j < image->width[i] * image->height[i]; j++)
        {
            int found = 0;
            for (k = 0; k < n_colors; k++)
            {
                if (p[i][j].r == colormap[k].Red &&
                    p[i][j].g == colormap[k].Green &&
                    p[i][j].b == colormap[k].Blue)
                {
                    found = 1;
                }
            }

            if (found == 0)
            {
                if (n_colors >= 256)
                {
                    fprintf(stderr,
                        "Error: Found too many colors inside the image\n"
                    );
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
    if (n_colors != (1 << GifBitSize(n_colors)))
    {
        n_colors = (1 << GifBitSize(n_colors));
    }

    /* Change the color map inside the animated gif */
    ColorMapObject* cmo;

    cmo = GifMakeMapObject(n_colors, colormap);
    if (cmo == NULL)
    {
        fprintf(stderr, "Error while creating a ColorMapObject w/ %d color(s)\n",
            n_colors);
        return 0;
    }

    image->g->SColorMap = cmo;

    /* Update the raster bits according to color map */
    for (i = 0; i < image->n_images; i++)
    {
        for (j = 0; j < image->width[i] * image->height[i]; j++)
        {
            int found_index = -1;
            for (k = 0; k < n_colors; k++)
            {
                if (p[i][j].r == image->g->SColorMap->Colors[k].Red &&
                    p[i][j].g == image->g->SColorMap->Colors[k].Green &&
                    p[i][j].b == image->g->SColorMap->Colors[k].Blue)
                {
                    found_index = k;
                }
            }

            if (found_index == -1)
            {
                fprintf(stderr,
                    "Error: Unable to find a pixel in the color map\n");
                return 0;
            }

            image->g->SavedImages[i].RasterBits[j] = found_index;
        }
    }
    /* Write the final image */
    if (!output_modified_read_gif(filename, image->g)) { return 0; }

    return 1;
}

void
apply_gray_filter(animated_gif* image)
{
    int i, j;
    pixel** p;

    p = image->p;

    for (i = 0; i < image->n_images; i++)
    {
        for (j = 0; j < image->width[i] * image->height[i]; j++)
        {
            int moy;

            moy = (p[i][j].r + p[i][j].g + p[i][j].b) / 3;
            if (moy < 0) moy = 0;
            if (moy > 255) moy = 255;

            p[i][j].r = moy;
            p[i][j].g = moy;
            p[i][j].b = moy;
        }
    }
}

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)
void
apply_blur_filter(animated_gif* image, int size, int threshold)
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
    for (i = 0; i < image->n_images; i++)
    {
        n_iter = 0;
        width = image->width[i];
        height = image->height[i];

        /* Allocate array of new pixels */
        new = (pixel*)malloc(width * height * sizeof(pixel));


        /* Perform at least one blur iteration */
        do
        {
            end = 1;
            n_iter++;


            for (j = 0; j < height - 1; j++)
            {
                for (k = 0; k < width - 1; k++)
                {
                    new[CONV(j, k, width)].r = p[i][CONV(j, k, width)].r;
                    new[CONV(j, k, width)].g = p[i][CONV(j, k, width)].g;
                    new[CONV(j, k, width)].b = p[i][CONV(j, k, width)].b;
                }
            }

            /* Apply blur on top part of image (10%) */
            for (j = size; j < height / 10 - size; j++)
            {
                for (k = size; k < width - size; k++)
                {
                    int stencil_j, stencil_k;
                    int t_r = 0;
                    int t_g = 0;
                    int t_b = 0;

                    for (stencil_j = -size; stencil_j <= size; stencil_j++)
                    {
                        for (stencil_k = -size; stencil_k <= size; stencil_k++)
                        {
                            t_r += p[i][CONV(j + stencil_j, k + stencil_k, width)].r;
                            t_g += p[i][CONV(j + stencil_j, k + stencil_k, width)].g;
                            t_b += p[i][CONV(j + stencil_j, k + stencil_k, width)].b;
                        }
                    }

                    new[CONV(j, k, width)].r = t_r / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
                }
            }

            /* Copy the middle part of the image */
            for (j = height / 10 - size; j < height * 0.9 + size; j++)
            {
                for (k = size; k < width - size; k++)
                {
                    new[CONV(j, k, width)].r = p[i][CONV(j, k, width)].r;
                    new[CONV(j, k, width)].g = p[i][CONV(j, k, width)].g;
                    new[CONV(j, k, width)].b = p[i][CONV(j, k, width)].b;
                }
            }

            /* Apply blur on the bottom part of the image (10%) */
            for (j = height * 0.9 + size; j < height - size; j++)
            {
                for (k = size; k < width - size; k++)
                {
                    int stencil_j, stencil_k;
                    int t_r = 0;
                    int t_g = 0;
                    int t_b = 0;

                    for (stencil_j = -size; stencil_j <= size; stencil_j++)
                    {
                        for (stencil_k = -size; stencil_k <= size; stencil_k++)
                        {
                            t_r += p[i][CONV(j + stencil_j, k + stencil_k, width)].r;
                            t_g += p[i][CONV(j + stencil_j, k + stencil_k, width)].g;
                            t_b += p[i][CONV(j + stencil_j, k + stencil_k, width)].b;
                        }
                    }

                    new[CONV(j, k, width)].r = t_r / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
                    new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
                }
            }

            for (j = 1; j < height - 1; j++)
            {
                for (k = 1; k < width - 1; k++)
                {

                    float diff_r;
                    float diff_g;
                    float diff_b;

                    diff_r = (new[CONV(j, k, width)].r - p[i][CONV(j, k, width)].r);
                    diff_g = (new[CONV(j, k, width)].g - p[i][CONV(j, k, width)].g);
                    diff_b = (new[CONV(j, k, width)].b - p[i][CONV(j, k, width)].b);

                    if (diff_r > threshold || -diff_r > threshold
                        ||
                        diff_g > threshold || -diff_g > threshold
                        ||
                        diff_b > threshold || -diff_b > threshold
                        ) {
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

void
apply_sobel_filter(animated_gif* image)
{
    int i, j, k;
    int width, height;

    pixel** p;

    p = image->p;

    for (i = 0; i < image->n_images; i++)
    {
        width = image->width[i];
        height = image->height[i];

        pixel* sobel;

        sobel = (pixel*)malloc(width * height * sizeof(pixel));

        for (j = 1; j < height - 1; j++)
        {
            for (k = 1; k < width - 1; k++)
            {
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


                if (val_blue > 50)
                {
                    sobel[CONV(j, k, width)].r = 255;
                    sobel[CONV(j, k, width)].g = 255;
                    sobel[CONV(j, k, width)].b = 255;
                }
                else
                {
                    sobel[CONV(j, k, width)].r = 0;
                    sobel[CONV(j, k, width)].g = 0;
                    sobel[CONV(j, k, width)].b = 0;
                }
            }
        }

        for (j = 1; j < height - 1; j++)
        {
            for (k = 1; k < width - 1; k++)
            {
                p[i][CONV(j, k, width)].r = sobel[CONV(j, k, width)].r;
                p[i][CONV(j, k, width)].g = sobel[CONV(j, k, width)].g;
                p[i][CONV(j, k, width)].b = sobel[CONV(j, k, width)].b;
            }
        }
        free(sobel);
    }
}

void serialize(animated_gif* image, int** serialized_images, int num_images) {
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

void deserialize(int* serialized_images, animated_gif* image, int num_images) {
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

int* get_image_from_serialized(int* serialized_images, int index) {
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

/* Creating MPI data structure to pixel and gif*/

void create_pixel_mpi_type(MPI_Datatype* pixel_mpi_type) {
    int block_lengths[3] = { 1, 1, 1 };
    MPI_Datatype types[3] = { MPI_INT, MPI_INT, MPI_INT };
    MPI_Aint offsets[3];
    offsets[0] = offsetof(pixel, r);
    offsets[1] = offsetof(pixel, g);
    offsets[2] = offsetof(pixel, b);

    MPI_Type_create_struct(3, block_lengths, offsets, types, pixel_mpi_type);
    MPI_Type_commit(pixel_mpi_type);
}

void create_animated_gif_mpi_type(MPI_Datatype* animated_gif_mpi_type, MPI_Datatype pixel_mpi_type) {
    int block_lengths[5] = { 1, 1, 1, 1, 1 };

    MPI_Datatype types[5] = { MPI_INT, MPI_INT, MPI_INT, MPI_DOUBLE, MPI_DOUBLE };
    MPI_Aint offsets[5];
    offsets[0] = offsetof(animated_gif, n_images);
    offsets[1] = offsetof(animated_gif, width);
    offsets[2] = offsetof(animated_gif, height);
    offsets[3] = offsetof(animated_gif, p);
    offsets[4] = offsetof(animated_gif, g);

    MPI_Type_create_struct(5, block_lengths, offsets, types, animated_gif_mpi_type);
    MPI_Type_commit(animated_gif_mpi_type);
}

void serialize(animated_gif* image, int** serialized_images, int num_images){
    int width = image->width[0];
    int height = image->height[0];
    *serialized_images = malloc(sizeof(int)*num_images*(3+3*width*height));
    for(int i = 0; i < num_images; i++){
        (*serialized_images)[i*width*height*3+0] = num_images;
        (*serialized_images)[i*width*height*3+1] = width;
        (*serialized_images)[i*width*height*3+2] = height;
        for(int j = 0; j < width * height * 3; j += 3) {
            (*serialized_images)[i*(width*height*3+3) + j + 3] = image->p[i][j / 3].r;
            (*serialized_images)[i*(width*height*3+3) + j + 4] = image->p[i][j / 3].g;
            (*serialized_images)[i*(width*height*3+3) + j + 5] = image->p[i][j / 3].b;
        }
    }
}

void deserialize(int* serialized_images, animated_gif* image, int num_images) {
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
animated_gif create_dummy_image() {
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
        {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}},
        {{255, 255, 0}, {255, 0, 255}, {0, 255, 255}},
        {{128, 128, 128}, {0, 0, 0}, {255, 255, 255}}
    };

    for (int i = 0; i < dummy.width[0] * dummy.height[0]; i++) {
        dummy.p[0][i].r = pixelValues[i / dummy.width[0]][i % dummy.width[0]][0];
        dummy.p[0][i].g = pixelValues[i / dummy.width[0]][i % dummy.width[0]][1];
        dummy.p[0][i].b = pixelValues[i / dummy.width[0]][i % dummy.width[0]][2];
    }

    return dummy;
}

bool is_dummy_image(const animated_gif* image) {
    if (image->n_images != 1) {
        return false;
    }

    if (image->width[0] != 3 || image->height[0] != 3) {
        return false;
    }

    int expectedPixelValues[3][3][3] = {
        {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}},
        {{255, 255, 0}, {255, 0, 255}, {0, 255, 255}},
        {{128, 128, 128}, {0, 0, 0}, {255, 255, 255}}
    };

    for (int i = 0; i < 3 * 3; i++) {
        if (image->p[0][i].r != expectedPixelValues[i / 3][i % 3][0] ||
            image->p[0][i].g != expectedPixelValues[i / 3][i % 3][1] ||
            image->p[0][i].b != expectedPixelValues[i / 3][i % 3][2]) {
            return false;
        }
    }

    return true;
}


/* DEBUG FUNCTIONS */
void print_first_pixel(animated_gif* gif, int rank) {
    if (gif == NULL) {
        printf("Error: Null pointer to animated_gif structure.\n");
        return;
    }

    for (int i = 0; i < gif->n_images; i++) {
        int width = gif->width[i];
        int height = gif->height[i];

        if (width > 0 && height > 0) {
            printf("antes do first pixel %d\n", rank);
            pixel first_pixel = gif->p[i][0];  

            printf("rank %d - Image %d - First Pixel (RGB): (%d, %d, %d)\n", rank, i + 1, first_pixel.r, first_pixel.g, first_pixel.b);
        } else {
            printf("Image %d - Invalid dimensions (width or height is <= 0).\n", i + 1);
        }
    }
}

/*
 * Main entry point
 */
int
main(int argc, char** argv)
{
    char* input_filename;
    char* output_filename;
    animated_gif* image = NULL;
    animated_gif* image_received;
    animated_gif* remainder_images;
    animated_gif* sent_images;

    int num_images;
    int* image_information = malloc(sizeof(int)*3);
    struct timeval t1, t2;
    double duration;
    int root_process = 0;
    int rank, size;
    FILE* fptr;

    MPI_Datatype pixel_mpi_type;
    MPI_Datatype animated_gif_mpi_type;

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
        if (image == NULL) { return 1; }
        /* IMPORT Timer stop */
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
        printf("GIF loaded from file %s with %d image(s) in %lf s\n",
            input_filename, image->n_images, duration);
        image_information[0] = image->n_images;
        image_information[1] = image->width[0];
        image_information[2] = image->height[0];
    }
    gettimeofday(&t1, NULL);
    int* serialized_image;
    // int num_serialized_images = num_images > elements_per_process * size ? elements_per_process * size : num_images;
    // int* received_serialized_image = malloc(sizeof(int) * elements_per_process * (3 + 3 * image_information[1] * image_information[2]));
    // if (rank == root_process) {
    //     // serialize(image, &serialized_image, num_serialized_images);
    //     int count = 0;
        
    // }
    // else if(rank == 1) {

    // }

    // animated_gif* deserialized_image = malloc(sizeof(animated_gif));
    // for (int i = 0; i < 20; i++) {
    //     printf("%d, %d\n", received_serialized_image[i], i);
    // }
    // deserialize(received_serialized_image, deserialized_image, elements_per_process);
    /* FILTER Timer start */

    /* Convert the pixels into grayscale */
    apply_gray_filter(image);

    /* Apply blur filter with convergence value */
    apply_blur_filter(image, 5, 20);

    /* Apply sobel filter on pixels */
    apply_sobel_filter(image);

    if (rank != root_process) {
        MPI_Finalize();
        return 0;
    }

    // NEED TO SERIALIZE/DESERIALIZE
    animated_gif** recv;
    recv = malloc(sizeof(animated_gif*)*size);
    MPI_Gather(
        image_received,
        elements_per_process,
        animated_gif_mpi_type,
        recv,
        elements_per_process,
        animated_gif_mpi_type,
        root_process,
        MPI_COMM_WORLD
    );
    if (rank != root_process) {
        MPI_Finalize();
        return 0;
    }
    for(int i = 0; i < size; i++){
        for(int j = 0; j < elements_per_process; j++) {
            image[j + i*elements_per_process].p = recv[i][j].p;
        }
    }
    if (remainder != 0) {
        for (int i = size * elements_per_process; i < num_images; i++) {
            image[i].p = remainder_images[i - size * elements_per_process].p;
        }
    }
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
    if (!store_pixels(output_filename, image)) { return 1; }

    /* EXPORT Timer stop */
    gettimeofday(&t2, NULL);

    duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
    free(image_information);
    if (rank == root_process) {
        free(serialized_image);
    }
    // free(received_serialized_image);
    printf("Export done in %lf s in file %s\n", duration, output_filename);
    MPI_Finalize();

    return 0;
}
