#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define MAX_DIV 10000000000000000000ULL
#define MAX_OFFSET 500
#define BUFF_SIZE (-181 + MAX_OFFSET * 109) / 1000 + 1

void print_bn(unsigned long long *digits, long long size)
{
    if (!digits || size <= 0) {
        return;
    }
    unsigned __int128 div = 0;
    for (int i = 0; i < size; i++) {
        div = 0;
        digits[size] = 0;
        for (int j = size - 1; j >= i; j--) {
            div = div << 64 | digits[j];
            digits[j + 1] = div / MAX_DIV;
            div = div % MAX_DIV;
        }
        digits[i] = div;
        size += digits[size] > 0;
    }

    printf("%llu", digits[size - 1]);
    for (int i = size - 2; i >= 0; i--) {
        printf("%019llu", digits[i]);
    }
    printf(".\n");
}


int main()
{
    long long sz;

    unsigned long long buf[BUFF_SIZE];
    char write_buf[] = "testing writing";
    int offset = MAX_OFFSET;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, sizeof(buf)) / sizeof(long long);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "",
               i);
        print_bn(buf, sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, sizeof(buf)) / sizeof(long long);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "",
               i);
        print_bn(buf, sz);
    }

    close(fd);
    return 0;
}
