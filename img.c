#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Структура изображения
typedef struct {
    unsigned char* data;    // Пиксели
    int width;              // Ширина 
    int height;             // Высота
    int channels;           // 1 (серый) или 3 (RGB)
} Image;

// Загрузка изображения из файла
Image* image_load(const char* filename) {
    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) return NULL;
    
    img->data = stbi_load(filename, &img->width, &img->height, &img->channels, 0);
    
    if (!img->data) {
        fprintf(stderr, "Ошибка загрузки: %s\n", filename);
        free(img);
        return NULL;
    }
    
    return img;
}

// Сохранение
int image_save(Image* img, const char* filename) {
    return stbi_write_png(filename, img->width, img->height, 
                          img->channels, img->data, img->width * img->channels);
}

// Очистка памяти
void image_free(Image* img) {
    if (img) {
        if (img->data) stbi_image_free(img->data);
        free(img);
    }
}

// Функция сравнения для qsort
static int compare_unsigned_char(const void* a, const void* b) {
    return (*(unsigned char*)a - *(unsigned char*)b);
}



// Медианный фильтр
Image* median_filter(Image* src, int kernel_size) {
    if (kernel_size % 2 == 0) {
        fprintf(stderr, "Ошибка: Размер ядра должен быть нечётным\n");
        return NULL;
    }
    
    Image* dst = (Image*)malloc(sizeof(Image));
    if (!dst) return NULL;
    
    dst->width = src->width;
    dst->height = src->height;
    dst->channels = src->channels;
    dst->data = (unsigned char*)malloc(src->width * src->height * src->channels);
    
    if (!dst->data) {
        free(dst);
        return NULL;
    }
    
    int offset = kernel_size / 2;
    int window_size = kernel_size * kernel_size;
    unsigned char* window = (unsigned char*)malloc(window_size * sizeof(unsigned char));
    
    if (!window) {
        free(dst->data);
        free(dst);
        return NULL;
    }
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            for (int c = 0; c < src->channels; c++) {
                int count = 0;
                
                for (int ky = -offset; ky <= offset; ky++) {
                    for (int kx = -offset; kx <= offset; kx++) {
                        int ix = x + kx;
                        int iy = y + ky;
                        
                        if (ix >= 0 && ix < src->width && iy >= 0 && iy < src->height) {
                            int idx = (iy * src->width + ix) * src->channels + c;
                            window[count++] = src->data[idx];
                        }
                    }
                }
                
                qsort(window, count, sizeof(unsigned char), compare_unsigned_char);
                unsigned char median = window[count / 2];
                
                int dst_idx = (y * dst->width + x) * dst->channels + c;
                dst->data[dst_idx] = median;
            }
        }
    }
    
    free(window);
    return dst;
}

// Гауссов фильтр
// Создание ядра
float* create_gaussian_kernel(int size, float sigma) {
    if (size % 2 == 0) size++;
    int c = size / 2;
    float* k = (float*)malloc(size * size * sizeof(float));
    float sum = 0;
    
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) {
            int x = i - c, y = j - c;
            float v = expf(-(x*x + y*y) / (2 * sigma * sigma));
            k[i * size + j] = v;
            sum += v;
        }
    
    for (int i = 0; i < size * size; i++) k[i] /= sum;
    return k;
}

// Свёртка
Image* convolution(Image* src, float* k, int ks) {
    if (ks % 2 == 0) return NULL;
    
    Image* dst = malloc(sizeof(Image));
    *dst = *src;
    dst->data = malloc(src->width * src->height * src->channels);
    if (!dst->data) { free(dst); return NULL; }
    
    int off = ks / 2;
    
    for (int y = 0; y < src->height; y++)
        for (int x = 0; x < src->width; x++)
            for (int c = 0; c < src->channels; c++) {
                float sum = 0;
                for (int ky = -off; ky <= off; ky++)
                    for (int kx = -off; kx <= off; kx++) {
                        int ix = x + kx, iy = y + ky;
                        if (ix >= 0 && ix < src->width && iy >= 0 && iy < src->height) {
                            int ki = (ky + off) * ks + (kx + off);
                            int pi = (iy * src->width + ix) * src->channels + c;
                            sum += k[ki] * src->data[pi];
                        }
                    }
                int di = (y * src->width + x) * src->channels + c;
                dst->data[di] = sum > 255 ? 255 : (sum < 0 ? 0 : sum);
            }
    return dst;
}

// Гауссов фильтр
Image* gaussian_filter(Image* src, int kernel_size, float sigma) {
    if (sigma <= 0.0f) {
        sigma = kernel_size / 3.0f;
    }
    
    float* kernel = create_gaussian_kernel(kernel_size, sigma);
    if (!kernel) return NULL;
    
    Image* dst = convolution(src, kernel, kernel_size);
    
    free(kernel);
    return dst;
}


int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s input.png output.png\n", argv[0]);
        printf("or\n");
        printf("Usage: %s input.png output.png\n", argv[0]);
        return 1;
    }
    
    Image* img = image_load(argv[1]);
    if (!img) return 1;

    printf("Enter: g / m <size> (Гауссов фильтр/Медианный фильтр)\n");
    char k[3];
    int s = 0;
    scanf("%s%d", k, &s);
    if (s % 2 == 0) s++;

    Image* res = NULL;
    if (strcmp(k, "m") == 0){
        res = median_filter(img, s);
    }
    else {
        res = gaussian_filter(img, s, 0.0f); 
    }

    if (!res) { image_free(img); return 1; };
    
    image_save(res, argv[2]);
    
    image_free(img);
    image_free(res);
    
    printf("Готово!\n");
    return 0;
}