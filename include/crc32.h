#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

void crc32_init(void);
uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t length);
uint32_t crc32_finalize(uint32_t crc);

#endif /* CRC32_H */
