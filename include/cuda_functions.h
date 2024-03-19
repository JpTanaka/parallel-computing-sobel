#ifndef CUDA_FUNCTIONS_H
#define CUDA_FUNCTIONS_H

void apply_blur_filter_cuda(int* image, int threshold, int size, int width, int height);
void apply_sobel_filter_cuda(int* image, int width, int height);
int is_cuda_available();

#endif // CUDA_FUNCTIONS_H
