#include "bitio.h"
#include <string.h>

void bitwriter_init(BitWriter *bw, FILE *fp) {
    bw->fp = fp;
    bw->io_pos = 0;
    bw->bit_buf = 0;
    bw->bit_count = 0;
    bw->total_bytes_written = 0;
    memset(bw->io_buf, 0, sizeof(bw->io_buf));
}

void bitwriter_write_bit(BitWriter *bw, int bit) {
    bw->bit_buf = (unsigned char)((bw->bit_buf << 1) | (bit & 1));
    bw->bit_count++;
    if (bw->bit_count == 8) {
        bw->io_buf[bw->io_pos++] = bw->bit_buf;
        bw->bit_buf = 0;
        bw->bit_count = 0;
        bw->total_bytes_written++;
        if (bw->io_pos == BITIO_BUFFER_SIZE) {
            fwrite(bw->io_buf, 1, BITIO_BUFFER_SIZE, bw->fp);
            bw->io_pos = 0;
        }
    }
}

void bitwriter_write_byte(BitWriter *bw, unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        bitwriter_write_bit(bw, (byte >> i) & 1);
    }
}

void bitwriter_flush(BitWriter *bw) {
    if (bw->bit_count > 0) {
        bw->bit_buf <<= (8 - bw->bit_count); // pad trailing bits with zeros
        bw->io_buf[bw->io_pos++] = bw->bit_buf;
        bw->bit_buf = 0;
        bw->bit_count = 0;
        bw->total_bytes_written++;
        if (bw->io_pos == BITIO_BUFFER_SIZE) {
            fwrite(bw->io_buf, 1, BITIO_BUFFER_SIZE, bw->fp);
            bw->io_pos = 0;
        }
    }
    if (bw->io_pos > 0) {
        fwrite(bw->io_buf, 1, bw->io_pos, bw->fp);
        bw->io_pos = 0;
    }
    fflush(bw->fp);
}

void bitreader_init(BitReader *br, FILE *fp) {
    br->fp = fp;
    br->io_size = 0;
    br->io_pos = 0;
    br->bit_buf = 0;
    br->bits_left = 0;
    memset(br->io_buf, 0, sizeof(br->io_buf));
}

int bitreader_read_bit(BitReader *br) {
    if (br->bits_left == 0) {
        if (br->io_pos >= br->io_size) {
            br->io_size = fread(br->io_buf, 1, BITIO_BUFFER_SIZE, br->fp);
            br->io_pos = 0;
            if (br->io_size == 0) {
                return -1; // EOF reached
            }
        }
        br->bit_buf = br->io_buf[br->io_pos++];
        br->bits_left = 8;
    }
    br->bits_left--;
    return (br->bit_buf >> br->bits_left) & 1;
}

int bitreader_read_byte(BitReader *br, unsigned char *out_byte) {
    unsigned char val = 0;
    for (int i = 0; i < 8; i++) {
        int b = bitreader_read_bit(br);
        if (b == -1) {
            return 0; // EOF or error
        }
        val = (unsigned char)((val << 1) | (b & 1));
    }
    *out_byte = val;
    return 1;
}
