#include <cuda.h>
#include <cuda_runtime.h>
#include <cstdio>

// #include "cuda_functions.h"

#define CONV(l, c, nb_c) \
(l) * (nb_c) + (c)

extern "C" void test() {
    int deviceCount;
    cudaError_t cuda_status = cudaGetDeviceCount(&deviceCount);
    int driverVersion;
    cudaError_t cudaStatus2 = cudaDriverGetVersion(&driverVersion);

    printf("%d", driverVersion);
    if (cudaStatus2 != cudaSuccess) {
        printf("driver failed: %s\n", cudaGetErrorString(cudaStatus2));
        return;
    }
    if (cuda_status != cudaSuccess) {
        printf("cudaGetDeviceCount failed: %s\n", cudaGetErrorString(cuda_status));
        return;
    }
}

extern "C" void test2(int* image) {
    int* d_image;
    cudaSetDevice(0);
    cudaMalloc(&d_image, sizeof(int));
    int end = 0;
    printf("before %d\n", image[0]);

    cudaError_t cuda_status = cudaMemcpy(d_image, &end, sizeof(int), cudaMemcpyHostToDevice);
    printf("error?: %s\n", cudaGetErrorString(cuda_status));

    cudaMemcpy(image, d_image, sizeof(int), cudaMemcpyDeviceToHost);
    printf("after %d\n", image[0]);
}

__global__ void apply_blur_filter_kernel(int* image, int* new_image, int *end, int size, int* threshold, int width, int height) {
    int j = threadIdx.y + blockDim.y * blockIdx.y;
    int i = threadIdx.x + blockDim.x * blockIdx.x;
    
    if (j >= 0 && j < height && i >= 0 && i < width) {
        new_image[j * width + i] = image[j * width + i];
        if (j >= size && j < height / 10 - size) {
            if (i >= size && i < width - size) {
                int stencil_j, stencil_k;
                int t = 0;

                for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                    for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                        t += image[(j + stencil_j) * width + (i + stencil_k)];
                    }
                }
                new_image[j * width + i] = t / ((2 * size + 1) * (2 * size + 1));
            }
        }

        if (j >= height * 0.9 + size && j < height - size) {
            if (i >= size && i < width - size) {
                int stencil_j, stencil_k;
                int t = 0;

                for (stencil_j = -size; stencil_j <= size; stencil_j++) {
                    for (stencil_k = -size; stencil_k <= size; stencil_k++) {
                        t += image[(j + stencil_j) * width + (i + stencil_k)];
                    }
                }

                new_image[j * width + i] = t / ((2 * size + 1) * (2 * size + 1));
            }
        }

        __syncthreads();

        if (j > 0 && j < height - 1 && i > 0 && i < width - 1) {
            float diff = new_image[j * width + i] - image[j * width + i];
            if (diff > *threshold || -diff > *threshold) {
                atomicAnd(end, 0);
            }
            image[j * width + i] = new_image[j * width + i];
        }
    }
}

extern "C" void apply_blur_filter_cuda(int* image, int threshold, int size, int width, int height) {
    int* d_image;
    int* d_new_image;
    int* d_threshold;
    int* d_end;
    int end = 0;

    cudaMalloc(&d_image, width * height * sizeof(int));
    cudaMalloc(&d_new_image, width * height * sizeof(int));
    cudaMalloc(&d_threshold, sizeof(int));
    cudaMalloc(&d_end, sizeof(int));
    cudaMemcpy(d_image, image, width * height * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_threshold, &threshold, sizeof(int), cudaMemcpyHostToDevice);
    dim3 block_size(32, 32);
    dim3 grid_size((width + block_size.x - 1) / block_size.x + 1, (height + block_size.y - 1) / block_size.y + 1, 1);

    do {
        end = 1;
        cudaMemcpy(d_end, &end, sizeof(int), cudaMemcpyHostToDevice);
        apply_blur_filter_kernel<<<grid_size, block_size>>>(d_image, d_new_image, d_end, size, d_threshold, width, height);

        cudaMemcpy(image, d_image, width * height * sizeof(int), cudaMemcpyDeviceToHost);
        cudaMemcpy(&end, d_end, sizeof(int), cudaMemcpyDeviceToHost);
        cudaMemcpy(&threshold, d_threshold, sizeof(int), cudaMemcpyDeviceToHost);

    } while (!end);
    cudaFree(d_image);
    cudaFree(d_new_image);
    cudaFree(d_threshold);
    cudaFree(d_end);
}



__global__ void sobel_filter_kernel(int* image, int* sobel, int width, int height) {
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (j > 0 && j < height - 1 && k > 0 && k < width - 1) {
        int pixel_no = image[CONV(j - 1, k - 1, width)];
        int pixel_n = image[CONV(j - 1, k, width)];
        int pixel_ne = image[CONV(j - 1, k + 1, width)];
        int pixel_so = image[CONV(j + 1, k - 1, width)];
        int pixel_s = image[CONV(j + 1, k, width)];
        int pixel_se = image[CONV(j + 1, k + 1, width)];
        int pixel_o = image[CONV(j, k - 1, width)];
        int pixel_e = image[CONV(j, k + 1, width)];
        float deltaX = -pixel_no + pixel_ne - 2 * pixel_o + 2 * pixel_e - pixel_so + pixel_se;
        float deltaY = pixel_se + 2 * pixel_s + pixel_so - pixel_ne - 2 * pixel_n - pixel_no;
        float val = sqrtf(deltaX * deltaX + deltaY * deltaY) / 4;
        if (val > 50) {
            sobel[CONV(j, k, width)] = 255;
        }
        else {
            sobel[CONV(j, k, width)] = 0;
        }
    }
    else if(((j==0 || j == height-1) && (k>=0 && k<width)) || ((k==0 || k == width-1) && (j>=0 && j<height-1))) {
        sobel[CONV(j, k, width)] = image[CONV(j, k, width)];
    }
}

extern "C" void apply_sobel_filter_cuda(int* image, int width, int height) {
    int* d_image = NULL;
    int* d_sobel = NULL;
    cudaError_t cuda_error = cudaMalloc((void**)&d_image, width * height * sizeof(int));
    if (cuda_error != cudaSuccess) {
        fprintf(stderr, "(cudaMalloc d_image): %s\n", cudaGetErrorString(cuda_error));
        return;
    }

    cuda_error = cudaMalloc((void**)&d_sobel, width * height * sizeof(int));
    if (cuda_error != cudaSuccess) {
        fprintf(stderr, "(cudaMalloc d_sobel): %s\n", cudaGetErrorString(cuda_error));
        cudaFree(d_image);  
        return;
    }

    cuda_error = cudaMemcpy(d_image, image, width * height * sizeof(int), cudaMemcpyHostToDevice);
    if (cuda_error != cudaSuccess) {
        fprintf(stderr, "(cudaMemcpy host to device): %s\n", cudaGetErrorString(cuda_error));
        cudaFree(d_image);  
        cudaFree(d_sobel);  
        return;
    }

    dim3 block_size(32, 32);
    // gridSize must be s.t. we can fit the whole image
    dim3 grid_size((width + block_size.x - 1) / block_size.x + 1, (height + block_size.y - 1) / block_size.y + 1);
    sobel_filter_kernel<<<grid_size, block_size>>>(d_image, d_sobel, width, height);
    cudaFree(d_image);
    cuda_error = cudaMemcpy(image, d_sobel, width*height * sizeof(int), cudaMemcpyDeviceToHost);
    if (cuda_error != cudaSuccess) {
        fprintf(stderr, "(cudaMemcpy device to host): %s\n", cudaGetErrorString(cuda_error));
        cudaFree(d_image);  
        cudaFree(d_sobel);  
        return;
    }
    cudaFree(d_sobel);
}

