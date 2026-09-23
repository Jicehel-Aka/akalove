#pragma once
#include "FreeRTOS.h"
typedef void* TaskHandle_t;
void vTaskDelay(TickType_t);
void vTaskDelete(TaskHandle_t);
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char*, uint32_t, void*, unsigned, TaskHandle_t*, BaseType_t);
