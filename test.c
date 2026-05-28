#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>  // Работа с папками
#include <sys/stat.h> // Существование папки
#include <time.h>
#include <sys/time.h>

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

// -------------------------------------------------------------------------------
// таймер
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// тестовое изображение
void create_test_image(const char* filename, int width, int height) {
    unsigned char* data = malloc(width * height * 3);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            data[idx] = (unsigned char)((x + y) % 256);
            data[idx+1] = (unsigned char)((x * 2) % 256);
            data[idx+2] = (unsigned char)((y * 3) % 256);
        }
    }
    stbi_write_png(filename, width, height, 3, data, width * 3);
    free(data);
}
// -------------------------------------------------------------------------------

// Загрузка изображения из файла
Image* image_load(const char* filename) {
    Image* img = malloc(sizeof(Image));
    if (!img) return NULL;
    
    img->data = stbi_load(filename, &img->width, &img->height, &img->channels, 0);
    
    if (!img->data) {
        fprintf(stderr, "Ошибка загрузки: %s\n", filename);
        free(img);
        return NULL;
    }
    return img;
}

//  Расширение файла
int ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t ls = strlen(str);
    size_t lsu = strlen(suffix);
    return lsu <= ls && strcasecmp(str + ls - lsu, suffix) == 0;
}

// Сохранение
int image_save(Image* img, const char* filename) {
    if (ends_with(filename, ".png")) {
        return stbi_write_png(filename, img->width, img->height, img->channels, img->data, img->width * img->channels);
    }
    else if (ends_with(filename, ".jpg") || ends_with(filename, ".jpeg")) {
        return stbi_write_jpg(filename, img->width, img->height, img->channels, img->data, 90);
    }
    else if (ends_with(filename, ".bmp")) {
        return stbi_write_bmp(filename, img->width, img->height, img->channels, img->data);
    }
    
    fprintf(stderr, "Неподдерживаемый формат: %s\n", filename);
    return 0;
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

Image* to_grayscale(Image* src) {
    Image* gray = malloc(sizeof(Image));
    gray->width = src->width;
    gray->height = src->height;
    gray->channels = 1;
    gray->data = malloc(src->width * src->height);
    
    if (src->channels == 1) {
        memcpy(gray->data, src->data, src->width * src->height);
        return gray;
    }
    
    for (int i = 0; i < src->width * src->height; i++) {
        unsigned char r = src->data[i * 3];
        unsigned char g = src->data[i * 3 + 1];
        unsigned char b = src->data[i * 3 + 2];
        gray->data[i] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    }
    return gray;
}

// Детекция границ
// Цветные границы
Image* edge_detection_overlay(Image* src, int thr, int r, int g, int b) {
    Image* res = malloc(sizeof(Image));
    *res = *src;
    res->data = malloc(src->width * src->height * src->channels);
    memcpy(res->data, src->data, src->width * src->height * src->channels);
    
    Image* gray = to_grayscale(src);
    if (!gray) {
        free(res->data);
        free(res);
        return NULL;
    }

    // Ядра Собеля
    int sobel_x[3][3] = {{-1, 0, 1},{-2, 0, 2},{-1, 0, 1}};
    int sobel_y[3][3] = {{-1, -2, -1},{0, 0, 0},{1, 2, 1}};
    
    for (int y = 1; y < src->height-1; y++)
        for (int x = 1; x < src->width-1; x++) {
            float gx = 0, gy = 0;

            for (int ky = -1; ky <= 1; ky++)
                for (int kx = -1; kx <= 1; kx++) {
                    int px = x + kx;
                    int py = y + ky;
                    float pixel = gray->data[py*gray->width + px];
                    gx += pixel * sobel_x[ky + 1][kx +1 ];
                    gy += pixel * sobel_y[ky + 1][kx + 1];
                }
            if (sqrtf(gx*gx + gy*gy) > thr) {
                if (src->channels == 1)// Черно белое
                    res->data[y*res->width + x] = 0.299*r + 0.587*g + 0.114*b;
                else { // Цветное
                    int idx = (y*res->width + x) * 3;
                    res->data[idx] = r;
                    res->data[idx+1] = g;
                    res->data[idx+2] = b;
                }
            }
        }
    image_free(gray);
    return res;
}

// Детекция границ
Image* edge_detection(Image* src, int threshold) {
    Image* gray = to_grayscale(src);
    if (!gray) return NULL;
    
    Image* edges = malloc(sizeof(Image));
    edges->width = gray->width;
    edges->height = gray->height;
    edges->channels = 1;
    edges->data = calloc(gray->width * gray->height, 1);
    
    if (!edges->data) {
        free(edges);
        image_free(gray);
        return NULL;
    }
    
    // Ядра Собеля
    int sobel_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int sobel_y[3][3] = {{-1, -2, -1}, { 0, 0, 0}, { 1, 2, 1}};

    for (int y = 1; y < gray->height - 1; y++) {
        for (int x = 1; x < gray->width - 1; x++) {
            float gx = 0, gy = 0;
            
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int px = x + kx;
                    int py = y + ky;
                    float pixel = gray->data[py*gray->width + px];
                    
                    gx += pixel * sobel_x[ky + 1][kx + 1];
                    gy += pixel * sobel_y[ky + 1][kx + 1];
                }
            }
            float magnitude = sqrtf(gx*gx + gy*gy);
            edges->data[y*edges->width + x] = (magnitude > threshold) ? 255 : 0;
        }
    }
    image_free(gray);
    return edges;
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

// Повышение резкости
Image* sharpen_filter(Image* src, float strength) {
    float kernel[9] = {0, -strength, 0, -strength, 1 + 4*strength, -strength, 0, -strength, 0};
    return convolution(src, kernel, 3);
}

// Проверка на папку
int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

// Получение списка изображений в папке
void get_image_files(const char* dir_path, char files[][256], int* count) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        printf("Ошибка: не удалось открыть папку %s\n", dir_path);
        return;
    }
    
    struct dirent* entry;
    *count = 0;
    while ((entry = readdir(dir)) != NULL && *count < 100) {
        const char* name = entry->d_name;
        if (ends_with(name, ".png") || ends_with(name, ".jpg") || ends_with(name, ".bmp")) {
            sprintf(files[(*count)++], "%s/%s", dir_path, name);
        }
    }
    closedir(dir);
}

// Имя выходного файла
void make_output_name(const char* input_path, const char* output_dir, const char* suffix, char* output_path) {
    const char* filename = strrchr(input_path, '/');
    filename = filename ? filename + 1 : input_path;
    int len = strcspn(filename, ".");
    sprintf(output_path, "%s/%.*s_%s.png", output_dir, len, filename, suffix);
}
// Выходная папка
void create_output_dir(const char* output_dir) {
    struct stat st;
    if (stat(output_dir, &st)){
        mkdir(output_dir, 0777);
    }
}

int main() {
    int sizes[][2] = {
        {320, 240},    // 0.07 MP
        {640, 480},    // 0.3 MP
        {1280, 720},   // 0.9 MP
        {1920, 1080},  // 2 MP
        {2560, 1440},  // 3.7 MP
        {3840, 2160}   // 8 MP
    };
    int num_sizes = 6;

    printf("+------------+--------------+-----------------+-----------------+-----------------+-----------------+\n");
    printf("| Размер     | Пикселей     | Медианный(3x3)  | Гауссов(3x3)    | Детекция(50)    | Резкость(2.0)  |\n");
    printf("+------------+--------------+-----------------+-----------------+-----------------+-----------------+\n");
    
    for (int s = 0; s < num_sizes; s++) {
        int width = sizes[s][0];
        int height = sizes[s][1];
        int pixels = width * height;
        char size_name[20];
        sprintf(size_name, "%dx%d", width, height);
        
        // Создаём тестовое изображение
        char filename[256];
        sprintf(filename, "test_%dx%d.png", width, height);
        create_test_image(filename, width, height);
        
        Image* img = image_load(filename);
        double start;
        
        // Медианный фильтр (3x3)
        start = get_time_ms();
        Image* res1 = median_filter(img, 3);
        double t_median = get_time_ms() - start;
        if (res1) image_free(res1);
        
        // Гауссов фильтр (3x3)
        start = get_time_ms();
        Image* res2 = gaussian_filter(img, 3, 0.0f);
        double t_gaussian = get_time_ms() - start;
        if (res2) image_free(res2);
        
        // Детекция границ (порог 50)
        start = get_time_ms();
        Image* res3 = edge_detection(img, 50);
        double t_edges = get_time_ms() - start;
        if (res3) image_free(res3);
        
        // Повышение резкости (сила 2.0)
        start = get_time_ms();
        Image* res4 = sharpen_filter(img, 2.0f);
        double t_sharpen = get_time_ms() - start;
        if (res4) image_free(res4);
        
        printf("| %-10s | %-12d | %15.2f | %15.2f | %15.2f | %15.2f |\n", 
               size_name, pixels, t_median, t_gaussian, t_edges, t_sharpen);
        
        image_free(img);
        remove(filename);
    }
    printf("+------------+--------------+-----------------+-----------------+-----------------+-----------------+\n");  
    return 0;
}