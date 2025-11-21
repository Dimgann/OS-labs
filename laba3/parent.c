#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024

typedef struct {
    size_t size;
    char buffer[BUFFER_SIZE];
} shm_data_t;

int main() {
    pid_t pid = getpid();

    char shm_name[64];
    char sem_parent_name[64];
    char sem_child_name[64];

    snprintf(shm_name, sizeof(shm_name), "/shm_%d", pid);
    snprintf(sem_parent_name, sizeof(sem_parent_name), "/sem_p_%d", pid);
    snprintf(sem_child_name, sizeof(sem_child_name), "/sem_c_%d", pid);

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    ftruncate(shm_fd, sizeof(shm_data_t));

    shm_data_t *shm = mmap(NULL, sizeof(shm_data_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, shm_fd, 0);

    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_parent = sem_open(sem_parent_name, O_CREAT, 0600, 0);
    sem_t *sem_child  = sem_open(sem_child_name,  O_CREAT, 0600, 0);

    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    char filename[BUFFER_SIZE];
    printf("Введите имя файла для записи: ");
    fgets(filename, BUFFER_SIZE, stdin);
    filename[strcspn(filename, "\n")] = 0;

    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    pid_t child = fork();
    if (child == 0) {
        execl("./child", "child",
              shm_name, sem_parent_name, sem_child_name, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }

    char input[BUFFER_SIZE];
    printf("Вводите строки:\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, BUFFER_SIZE, stdin))
            break;

        if (strcmp(input, "exit\n") == 0)
            break;

        strncpy(shm->buffer, input, BUFFER_SIZE);
        shm->size = strlen(input);

        sem_post(sem_child);
        sem_wait(sem_parent);

        if (strncmp(shm->buffer, "ERROR", 5) == 0) {
            printf("Ошибка от child: %s", shm->buffer);
        } else {
            write(file_fd, shm->buffer, shm->size);
        }
    }

    close(file_fd);
    wait(NULL);

    munmap(shm, sizeof(shm_data_t));
    shm_unlink(shm_name);

    sem_close(sem_parent);
    sem_close(sem_child);
    sem_unlink(sem_parent_name);
    sem_unlink(sem_child_name);

    return 0;
}
