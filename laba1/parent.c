#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

int main() {
    int pipe1_fd[2];
    int pipe2_fd[2];
    pid_t pid;
    char filename[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    int file_fd;

    if (pipe(pipe1_fd) == -1 || pipe(pipe2_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    printf("Введите имя файла для записи: ");
    fflush(stdout);
    if (fgets(filename, BUFFER_SIZE, stdin) == NULL) {
        perror("fgets");
        exit(EXIT_FAILURE);
    }
    filename[strcspn(filename, "\n")] = 0;

    file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(pipe1_fd[1]);
        close(pipe2_fd[0]);

        dup2(pipe1_fd[0], STDIN_FILENO);
        close(pipe1_fd[0]);

        dup2(pipe2_fd[1], STDOUT_FILENO);
        close(pipe2_fd[1]);

        execl("./child", "child", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        close(pipe1_fd[0]);
        close(pipe2_fd[1]);

        printf("Вводите строки:\n");
        while (1) {
            printf("> ");
            fflush(stdout);
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                perror("fgets");
                break;
            }

            if (strcmp(buffer, "exit\n") == 0) {
                break;
            }

            write(pipe1_fd[1], buffer, strlen(buffer));

            ssize_t bytes_read = read(pipe2_fd[0], buffer, BUFFER_SIZE - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                if (strstr(buffer, "ERROR") != NULL) {
                    printf("Ошибка от child: %s", buffer);
                } else {
                    write(file_fd, buffer, strlen(buffer));
                }
            }
        }

        close(pipe1_fd[1]);
        close(pipe2_fd[0]);
        close(file_fd);
        wait(NULL);
    }

    return 0;
}