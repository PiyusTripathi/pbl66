#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdint.h>
#define HUFF_MAGIC_0 'H'
#define HUFF_MAGIC_1 'U'
#define HUFF_MAGIC_2 'F'
#define HUFF_MAGIC_3 '1'
#define HUFF_VERSION 1

#define MAX_CODE_BITS 260

typedef struct HuffmanNode {
  unsigned char symbol;
  uint64_t freq;
  struct HuffmanNode *left;
  struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
  char bits[MAX_CODE_BITS];
  int len;
} HuffmanCode;

typedef struct {
  uint64_t original_size;
  uint64_t compressed_size;
  uint32_t original_crc;
  uint32_t decompressed_crc;
  double compression_ratio; // Percentage space saved: (1.0 -
                            // compressed/original) * 100.0
  double elapsed_time_ms;
  int unique_symbols;
} HuffmanStats;

typedef enum {
  HUFF_SUCCESS = 0,
  HUFF_ERROR_FILE_OPEN,
  HUFF_ERROR_FILE_READ,
  HUFF_ERROR_FILE_WRITE,
  HUFF_ERROR_INVALID_FORMAT,
  HUFF_ERROR_CORRUPTED_DATA,
  HUFF_ERROR_CRC_MISMATCH,
  HUFF_ERROR_OUT_OF_MEMORY
} HuffmanResult;

const char *huffman_result_to_string(HuffmanResult res);

HuffmanResult huffman_compress_file(const char *input_path,
                                    const char *output_path,
                                    HuffmanStats *stats);
HuffmanResult huffman_decompress_file(const char *input_path,
                                      const char *output_path,
                                      HuffmanStats *stats);
HuffmanResult huffman_verify_file(const char *compressed_path,
                                  HuffmanStats *stats);

#endif /* HUFFMAN_H */
