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




int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);


    if (rank == 0) {
        int width = 10;
        int height = 8;

        int **image = (int **) malloc(height * sizeof(int *));
        if (image == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        for (int i = 0; i < height; i++) {
            image[i] = (int *) malloc(width * sizeof(int));
            if (image[i] == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                for (int j = 0; j < i; j++) {
                    free(image[j]);
                }
                free(image);
                return 1;
            }
        }

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                image[i][j] = j;
            }
        }


        int widths[size];
        int heights[size];

        calculatePatchSizes(width, height, size, widths, heights);

        printf("Patch Sizes:\n");
        for (int i = 0; i < size; i++) {
            printf("Process %d: Width = %d, Height = %d\n", i, widths[i], heights[i]);
        }




        printf("Initial image:\n");
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                printf("%d ", image[i][j]);
            }
            printf("\n");
        }


        for (int patch_nbr = 1; patch_nbr < size; ++patch_nbr) {
            int patch_width = widths[patch_nbr] + 2; //+2 to store padding
            int patch_height = heights[patch_nbr] + 2;
            int **patch = (int **) malloc(patch_height * sizeof(int *));
            if (patch == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return 1;
            }

            for (int i = 0; i < patch_height; i++) {
                patch[i] = (int *) malloc(patch_width * sizeof(int));
                if (patch[i] == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    for (int j = 0; j < i; j++) {
                        free(patch[j]);
                    }
                    free(patch);
                    return 1;
                }
            }

            for (int i = 0; i < patch_height; i++) {
                for (int j = 0; j < patch_width; j++) {
                    patch[i][j] = 0;
                }
            }
            extractPatch(image, width, height, patch, widths, heights, patch_nbr, size);
//            printf("Image patch:\n");
//            for (int i = 0; i < patch_height; i++) {
//                for (int j = 0; j < patch_width; j++) {
//                    printf("%d ", patch[i][j]);
//                }
//                printf("\n");
//            }
            int hw[2] = {patch_height, patch_width};
            MPI_Send(hw, 2, MPI_INT, patch_nbr, 0, MPI_COMM_WORLD);

            for (int i = 0; i < heights[patch_nbr]; i++) {
                MPI_Send(patch[i], widths[patch_nbr], MPI_INT, patch_nbr, 0, MPI_COMM_WORLD);
            }

            for (int i = 0; i < patch_height; i++) {
                free(patch[i]);
            }
            free(patch);

        }


        //    printf("New 2D Array:\n");
        //    for (int i = 0; i < patch_height; i++) {
        //        for (int j = 0; j < patch_width; j++) {
        //            printf("%d ", patch[i][j]);
        //        }
        //        printf("\n");
        //    }


        for (int i = 0; i < height; i++) {
            free(image[i]);
        }
        free(image);

    } else {
        printf("%d", rank);
        int hw[2];
        MPI_Recv(hw, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int height = hw[0];
        int width = hw[1];

        int patch_width = width;
        int patch_height = height;
        int **patch = (int **) malloc(patch_height * sizeof(int *));
        if (patch == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        for (int i = 0; i < patch_height; i++) {
            patch[i] = (int *) malloc(patch_width * sizeof(int));
            if (patch[i] == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                for (int j = 0; j < i; j++) {
                    free(patch[j]);
                }
                free(patch);
                return 1;
            }
        }

        for (int i = 0; i < patch_height; i++) {
            for (int j = 0; j < patch_width; j++) {
                patch[i][j] = 0;
            }
        }


        for (int i = 0; i < height; i++) {
            MPI_Recv(patch[i], width, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        printf("Process %d received the patch:\n", rank);
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                printf("%d ", patch[i][j]);
            }
            printf("\n");
        }
    }

    MPI_Finalize();
    return 0;
}
