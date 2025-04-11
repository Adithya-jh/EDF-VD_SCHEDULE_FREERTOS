#ifndef CUSTOM_APIS_H
#define CUSTOM_APIS_H

#include <stdint.h>

/**
 * @brief Returns a random temperature between 10 and 90.
 */
int32_t getTemperature(void);

/**
 * @brief Returns a random pressure between 2 and 10.
 */
int32_t getPressure(void);

/**
 * @brief Returns a random height between 100 and 1000.
 */
int32_t getHeight(void);

#endif /* CUSTOM_APIS_H */
