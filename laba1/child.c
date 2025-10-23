#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

int main() {
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            break;
        }
        buffer[bytes_read] = '\0';

        if (isupper((unsigned char)buffer[0])) {
            write(STDOUT_FILENO, buffer, bytes_read);
        } else {
            char error_msg[BUFFER_SIZE];
            char truncated_buffer[256];
            int len = bytes_read < 200 ? bytes_read : 200;
            strncpy(truncated_buffer, buffer, len);
            truncated_buffer[len] = '\0';
            
            snprintf(error_msg, BUFFER_SIZE, 
                     "ERROR: Строка не начинается с заглавной буквы: %s\n", 
                     truncated_buffer);
            write(STDOUT_FILENO, error_msg, strlen(error_msg));
        }
    }

    return 0;
}