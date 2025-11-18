#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct {
    int id;
    int start_row;
    int end_row;
    int rows;
    int cols;
    int window_size;
    int total_iterations;
    float** src_matrix;
    float** dst_matrix;
    float* kernel;
    
    // Примитивы синхронизации без barrier
    pthread_mutex_t* iter_mutex;
    pthread_cond_t* iter_cond;
    int* current_iter;
    int* threads_ready;
    int num_threads;
} ThreadData;

float* create_kernel(int size) {
    float* kernel = malloc(size * size * sizeof(float));
    float sum = 0.0f;
    
    for (int i = 0; i < size * size; i++) {
        kernel[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        sum += fabsf(kernel[i]);
    }
    
    if (sum > 0) {
        for (int i = 0; i < size * size; i++) {
            kernel[i] /= sum;
        }
    }
    
    return kernel;
}

float** create_matrix(int rows, int cols) {
    float** matrix = malloc(rows * sizeof(float*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = malloc(cols * sizeof(float));
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = (float)rand() / RAND_MAX;
        }
    }
    return matrix;
}

void free_matrix(float** matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

float** copy_matrix(float** src, int rows, int cols) {
    float** dst = malloc(rows * sizeof(float*));
    for (int i = 0; i < rows; i++) {
        dst[i] = malloc(cols * sizeof(float));
        memcpy(dst[i], src[i], cols * sizeof(float));
    }
    return dst;
}

void apply_filter_row(int row, int rows, int cols, int window_size, 
                      float** src, float** dst, float* kernel) {
    int offset = window_size / 2;
    
    if (row < offset || row >= rows - offset) {
        for (int j = 0; j < cols; j++) {
            dst[row][j] = src[row][j];
        }
        return;
    }
    
    for (int j = offset; j < cols - offset; j++) {
        float sum = 0.0f;
        int kernel_idx = 0;
        
        for (int ki = -offset; ki <= offset; ki++) {
            for (int kj = -offset; kj <= offset; kj++) {
                sum += src[row + ki][j + kj] * kernel[kernel_idx];
                kernel_idx++;
            }
        }
        dst[row][j] = sum;
    }
    
    for (int j = 0; j < offset; j++) {
        dst[row][j] = src[row][j];
    }
    for (int j = cols - offset; j < cols; j++) {
        dst[row][j] = src[row][j];
    }
}

void* thread_func(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    for (int iter = 0; iter < data->total_iterations; iter++) {
        // Выполняем свою часть работы
        for (int i = data->start_row; i < data->end_row; i++) {
            apply_filter_row(i, data->rows, data->cols, data->window_size,
                           data->src_matrix, data->dst_matrix, data->kernel);
        }
        
        // Сообщаем о завершении итерации
        pthread_mutex_lock(data->iter_mutex);
        (*data->threads_ready)++;
        
        if (*data->threads_ready == data->num_threads) {
            // Все потоки завершили - меняем матрицы
            float** temp = data->src_matrix;
            data->src_matrix = data->dst_matrix;
            data->dst_matrix = temp;
            
            (*data->current_iter)++;
            *data->threads_ready = 0;
            
            // Будим все потоки
            pthread_cond_broadcast(data->iter_cond);
        } else {
            // Ждем завершения других потоков
            while (*data->current_iter <= iter) {
                pthread_cond_wait(data->iter_cond, data->iter_mutex);
            }
        }
        
        pthread_mutex_unlock(data->iter_mutex);
    }
    
    return NULL;
}

void sequential_filter(float** src, float** dst, int rows, int cols, 
                       int window_size, int iterations, float* kernel) {
    float** current = src;
    float** next = dst;
    
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < rows; i++) {
            apply_filter_row(i, rows, cols, window_size, current, next, kernel);
        }
        
        if (iter < iterations - 1) {
            float** temp = current;
            current = next;
            next = temp;
        }
    }
}

void parallel_filter(float** src, float** dst, int rows, int cols,
                     int window_size, int iterations, float* kernel, int num_threads) {
    // Создаем копии для работы
    float** src_copy = copy_matrix(src, rows, cols);
    float** dst_copy = create_matrix(rows, cols);
    
    // Инициализация примитивов синхронизации (важно для TSan!)
    pthread_mutex_t iter_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t iter_cond = PTHREAD_COND_INITIALIZER;
    
    int current_iter = 0;
    int threads_ready = 0;
    
    ThreadData* thread_data = malloc(num_threads * sizeof(ThreadData));
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    
    int rows_per_thread = rows / num_threads;
    int extra_rows = rows % num_threads;
    int start_row = 0;
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].id = i;
        thread_data[i].start_row = start_row;
        thread_data[i].end_row = start_row + rows_per_thread + (i < extra_rows ? 1 : 0);
        thread_data[i].rows = rows;
        thread_data[i].cols = cols;
        thread_data[i].window_size = window_size;
        thread_data[i].total_iterations = iterations;
        thread_data[i].src_matrix = src_copy;
        thread_data[i].dst_matrix = dst_copy;
        thread_data[i].kernel = kernel;
        thread_data[i].iter_mutex = &iter_mutex;
        thread_data[i].iter_cond = &iter_cond;
        thread_data[i].current_iter = &current_iter;
        thread_data[i].threads_ready = &threads_ready;
        thread_data[i].num_threads = num_threads;
        
        start_row = thread_data[i].end_row;
        
        pthread_create(&threads[i], NULL, thread_func, &thread_data[i]);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Копируем результат
    for (int i = 0; i < rows; i++) {
        memcpy(dst[i], dst_copy[i], cols * sizeof(float));
    }
    
    // Освобождаем ресурсы
    pthread_mutex_destroy(&iter_mutex);
    pthread_cond_destroy(&iter_cond);
    free_matrix(src_copy, rows);
    free_matrix(dst_copy, rows);
    free(thread_data);
    free(threads);
}

double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void run_performance_test(int matrix_size, int window_size, int iterations, int max_threads) {
    printf("\n===================================================\n");
    printf("Тестирование с параметрами:\n");
    printf("Размер матрицы: %d x %d\n", matrix_size, matrix_size);
    printf("Размер окна: %d x %d\n", window_size, window_size);
    printf("Количество наложений фильтра: %d\n", iterations);
    printf("===================================================\n");
    
    printf("\n| Потоки | Время (мс) | Ускорение | Эффективность |\n");
    printf("|--------|------------|-----------|---------------|\n");
    
    float** src_matrix = create_matrix(matrix_size, matrix_size);
    float** dst_matrix = create_matrix(matrix_size, matrix_size);
    float* kernel = create_kernel(window_size);
    
    double time_1_thread = 0.0;
    
    for (int num_threads = 1; num_threads <= max_threads; num_threads++) {
        float** src_copy = copy_matrix(src_matrix, matrix_size, matrix_size);
        float** dst_copy = create_matrix(matrix_size, matrix_size);
        
        double start_time = get_time_ms();
        
        if (num_threads == 1) {
            sequential_filter(src_copy, dst_copy, matrix_size, matrix_size, 
                            window_size, iterations, kernel);
        } else {
            parallel_filter(src_copy, dst_copy, matrix_size, matrix_size,
                          window_size, iterations, kernel, num_threads);
        }
        
        double end_time = get_time_ms();
        double elapsed = end_time - start_time;
        
        if (num_threads == 1) {
            time_1_thread = elapsed;
        }
        
        double speedup = time_1_thread / elapsed;
        double efficiency = speedup / num_threads;
        
        printf("| %6d | %10.2f | %9.2f | %13.2f |\n", 
               num_threads, elapsed, speedup, efficiency);
        fflush(stdout);
        
        free_matrix(src_copy, matrix_size);
        free_matrix(dst_copy, matrix_size);
    }
    
    free_matrix(src_matrix, matrix_size);
    free_matrix(dst_matrix, matrix_size);
    free(kernel);
}

int main(int argc, char* argv[]) {
    int max_threads = 6;
    int matrix_size = 500;
    int window_size = 3;
    int filter_iterations = 5;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            max_threads = atoi(argv[i + 1]);
            if (max_threads < 1) max_threads = 1;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            matrix_size = atoi(argv[i + 1]);
            if (matrix_size < 10) matrix_size = 10;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            window_size = atoi(argv[i + 1]);
            if (window_size < 1) window_size = 1;
            if (window_size % 2 == 0) window_size++;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            filter_iterations = atoi(argv[i + 1]);
            if (filter_iterations < 1) filter_iterations = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Использование: %s [-t потоки] [-s размер] [-w окно] [-i итерации]\n", argv[0]);
            printf("Пример: %s -t 4 -s 1000 -w 3 -i 10\n", argv[0]);
            return 0;
        }
    }
    
    printf("Параметры: матрица %dx%d, окно %d, %d итераций, до %d потоков\n", 
           matrix_size, matrix_size, window_size, filter_iterations, max_threads);
    
    srand((unsigned int)time(NULL));
    run_performance_test(matrix_size, window_size, filter_iterations, max_threads);
    
    return 0;
}