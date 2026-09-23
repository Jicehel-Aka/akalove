// Faux en-tête ESP-IDF : sert UNIQUEMENT à vérifier la syntaxe et les signatures (tools/check_hal_aka.sh).
#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef uint8_t StackType_t;
typedef void (*TaskFunction_t)(void*);
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portTICK_PERIOD_MS 1
#define IRAM_ATTR
typedef enum { GPIO_NUM_0 = 0 } gpio_num_t;
