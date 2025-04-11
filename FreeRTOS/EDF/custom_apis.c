#include "custom_apis.h"
#include <stdlib.h>
#include <time.h>

static int seedInitialized = 0;

/* Initialize random seed only once. */
static void initRandomSeed(void)
{
    if (!seedInitialized)
    {
        /* On a real embedded system without time(), pick a fixed or hardware-based seed. */
        srand((unsigned int)time(NULL));
        seedInitialized = 1;
    }
}

int32_t getTemperature(void)
{
    initRandomSeed();
    /* Range: 10..90  (81 values + 10 offset) */
    return (rand() % 81) + 10;
}

int32_t getPressure(void)
{
    initRandomSeed();
    /* Range: 2..10   (9 values + 2 offset) */
    return (rand() % 9) + 2;
}

int32_t getHeight(void)
{
    initRandomSeed();
    /* Range: 100..1000  (901 values + 100 offset) */
    return (rand() % 901) + 100;
}
