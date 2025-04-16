/* Define _POSIX_C_SOURCE to ensure POSIX functions like getcwd are declared */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // for getcwd()
#include <limits.h>   // for PATH_MAX
#include "sim_offline_edfvd.h"  // Declaration for vRunOfflineEDFVD()

int main(void)
{
    char cwd[PATH_MAX];

    printf("DEBUG: Entering main()...\n");
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("DEBUG: Current working directory: %s\n", cwd);
    } else {
        perror("DEBUG: getcwd() error");
    }

    printf("DEBUG: Running offline EDF-VD simulation directly (bypassing FreeRTOS scheduler)...\n");
    vRunOfflineEDFVD();
    printf("DEBUG: Offline simulation completed.\n");
    
    return EXIT_SUCCESS;
}
