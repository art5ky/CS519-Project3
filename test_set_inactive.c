#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#define SYS_set_inactive 449

int main(void)
{
    long ret;

    ret = syscall(SYS_set_inactive, 1);
    printf("set_inactive(1) returned %ld", ret);
    if (ret < 0)
        printf(" errno=%d (%s)", errno, strerror(errno));
    printf("\n");

    ret = syscall(SYS_set_inactive, 0);
    printf("set_inactive(0) returned %ld", ret);
    if (ret < 0)
        printf(" errno=%d (%s)", errno, strerror(errno));
    printf("\n");

    ret = syscall(SYS_set_inactive, 2);
    printf("set_inactive(2) returned %ld", ret);
    if (ret < 0)
        printf(" errno=%d (%s)", errno, strerror(errno));
    printf("\n");

    return 0;
}