// stb_impl.cpp — implémentation de stb_image (PNG seulement). Compilé sans avertissements (code tiers) :
// voir set_source_files_properties dans les CMakeLists.
// Allocations par hal::alloc : gros tampons en PSRAM sur la console (politique d'ESP-IDF).
#include "hal.h"

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS
#define STBI_MALLOC(sz) hal::alloc(sz)
#define STBI_REALLOC(p, sz) hal::realloc_((p), (sz))
#define STBI_FREE(p) hal::free_(p)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
