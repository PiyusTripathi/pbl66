#include "crc32.h"

static uint32_t s_crc32_table[256];
static int s_crc32_initialized = 0;

void crc32_init(void) {
    if (s_crc32_initialized) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xEDB88320U ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        s_crc32_table[i] = c;
    }
    s_crc32_initialized = 1;
}

uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t length) {
    if (!s_crc32_initialized) {
        crc32_init();
    }
    for (size_t i = 0; i < length; i++) {
        crc = s_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

uint32_t crc32_finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFFU;
}
