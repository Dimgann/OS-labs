#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024

typedef struct {
    size_t size;
    char buffer[BUFFER_SIZE];
} shm_data_t;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: child <shm> <sem_parent> <sem_child>\n");
        exit(EXIT_FAILURE);
    }

    const char *shm_name = argv[1];
    const char *sem_parent_name = argv[2];
    const char *sem_child_name = argv[3];

    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    shm_data_t *shm = mmap(NULL, sizeof(shm_data_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, shm_fd, 0);

    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_parent = sem_open(sem_parent_name, 0);
    sem_t *sem_child  = sem_open(sem_child_name,  0);

    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        sem_wait(sem_child);

        if (isupper((unsigned char)shm->buffer[0])) {

        } else {
            char truncated[200];
            strncpy(truncated, shm->buffer, 199);
            truncated[199] = '\0';

            snprintf(shm->buffer, BUFFER_SIZE,
                     "ERROR: Строка не начинается с заглавной буквы: %s\n",
                     truncated);
            shm->size = strlen(shm->buffer);
        }

        sem_post(sem_parent);
    }

    munmap(shm, sizeof(shm_data_t));
    sem_close(sem_parent);
    sem_close(sem_child);

    return 0;
}
