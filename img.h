#ifndef IMGPROC_H
#define IMGPROC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

// Структура изображения
typedef struct {
    unsigned char* data;    // Пиксели
    int width;              // Ширина 
    int height;             // Высота
    int channels;           // 1 (серый) или 3 (RGB)
} Image;

// Основные функции
// Загрузка изображения из файла
Image* image_load(const char* filename);
//  Расширение файла
int ends_with(const char* str, const char* suffix);
// Сохранение
int image_save(Image* img, const char* filename);
// Очистка памяти
void image_free(Image* img);
// Функция сравнения для qsort
static int compare_unsigned_char(const void* a, const void* b);


// Преобразование в оттенки серого
Image* to_grayscale(Image* src);

// Медианный фильтр
Image* median_filter(Image* src, int kernel_size);

// Гауссов фильтр
Image* gaussian_filter(Image* src, int kernel_size, float sigma);
// Создание ядра
float* create_gaussian_kernel(int size, float sigma);

// Свёртка
Image* convolution(Image* src, float* k, int ks);

// Повышение резкости
Image* sharpen_filter(Image* src, float strength);


// Детекция границ
// Черно-белая обработка
Image* edge_detection(Image* src, int threshold);
// Цветные границы
Image* edge_detection_overlay(Image* src, int thr, int r, int g, int b);


// Работа с папками
// Проверка на папку
int is_directory(const char* path);
// Получение списка изображений в папке
void get_image_files(const char* dir_path, char files[][256], int* count);
// Имя выходного файла
void make_output_name(const char* input_path, const char* output_dir, const char* suffix, char* output_path);
// Выходная папка
void create_output_dir(const char* output_dir);

#endif