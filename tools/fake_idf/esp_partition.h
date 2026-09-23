#pragma once
typedef struct esp_partition_s esp_partition_t;
typedef enum { ESP_PARTITION_TYPE_APP = 0 } esp_partition_type_t;
typedef enum { ESP_PARTITION_SUBTYPE_APP_OTA_1 = 0x11 } esp_partition_subtype_t;
const esp_partition_t* esp_partition_find_first(esp_partition_type_t, esp_partition_subtype_t, const char*);
