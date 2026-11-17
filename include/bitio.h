#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define BITIO_BUFFER_SIZE 65536

typedef struct {
    FILE *fp;
    unsigned char io_buf[BITIO_BUFFER_SIZE];
    size_t io_pos;
    unsigned char bit_buf;
    int bit_count; // Number of bits currently queued in bit_buf (0..7)
    uint64_t total_bytes_written;
} BitWriter;

typedef struct {
    FILE *fp;
    unsigned char io_buf[BITIO_BUFFER_SIZE];
    size_t io_size;
    size_t io_pos;
    unsigned char bit_buf;
    int bits_left; // Number of unread bits in bit_buf (0..8)
} BitReader;

void bitwriter_init(BitWriter *bw, FILE *fp);
void bitwriter_write_bit(BitWriter *bw, int bit);
void bitwriter_write_byte(BitWriter *bw, unsigned char byte);
void bitwriter_flush(BitWriter *bw);

void bitreader_init(BitReader *br, FILE *fp);
int bitreader_read_bit(BitReader *br);
int bitreader_read_byte(BitReader *br, unsigned char *out_byte);

#endif /* BITIO_H */
