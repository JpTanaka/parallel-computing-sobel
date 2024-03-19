#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>


void calculatePatchSizes(int width, int height, int nbr_patches, int widths[], int heights[]) {

    if (nbr_patches < 4) {
        int remaining_width = width % nbr_patches;
        for (int i = 0; i < nbr_patches; i++) {
            widths[i] = width / nbr_patches;
            heights[i] = height;
        }
        widths[nbr_patches - 1] += remaining_width;
        return;
    }

    double nbr_patches_root = sqrt(nbr_patches);
    if (nbr_patches_root == (int)nbr_patches_root){
        int remaining_width = width % (int)nbr_patches_root;
        int remaining_height = height % (int)nbr_patches_root;
        int count = 1;
        for (int i = 0; i < nbr_patches; i++) {
            widths[i] = width / nbr_patches_root;
            heights[i] = height / nbr_patches_root;
            if (count % (int)nbr_patches_root == 0){
                widths[i] += remaining_width;
            }
            if (count > nbr_patches - nbr_patches_root){
                heights[i] += remaining_height;
            }
            count++;
        }
        return;
    }
}

void extractPatch(int** image,int image_width, int image_height, int** patch, int* widths, int* heights, int patch_nbr, int nbr_patches){
    int start_i = 0;
    int start_j = 0;
    int nbr_patches_root = (int)sqrt(nbr_patches);
    int row = (int) patch_nbr / nbr_patches_root;
    for (int j = row * nbr_patches_root; j < patch_nbr; ++j) {
        start_j += widths[j];
    }
    for (int i = 1; i < patch_nbr; ++i) {
        if ((i + 1) % nbr_patches_root == 0){
            start_i += heights[i];
        }
    }

    for (int i = start_i; i < start_i + heights[patch_nbr]; ++i) {
        for (int j = start_j; j < start_j + widths[patch_nbr]; ++j) {
            patch[i - start_i + 1][j - start_j + 1] = image[i][j];
            // Lower neighbor
            if ((i + 1 >= start_i + heights[patch_nbr]) && (i + 1 < image_height)){
                patch[i - start_i + 2][j - start_j + 1] = image[i + 1][j];
            }
            // Higher neighbor
            if ((i - 1 < start_i) && (i - 1 > 0)){
                patch[0][j - start_j + 1] = image[i - 1][j];
            }
        }
        // Right neighbor
        if (start_j + heights[patch_nbr] <= image_width){
            patch[i - start_i + 1][heights[patch_nbr] + 1] = image[i][start_j + heights[patch_nbr]];
        }
        //Left neighbor
        if (start_j - 1 > 0){
            patch[i - start_i + 1][0] = image[i][start_j - 1];
        }
    }
}


