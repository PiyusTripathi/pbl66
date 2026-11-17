#include "huffman.h"
#include "bitio.h"
#include "crc32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <io.h>
#endif

/* --- Endianness Helpers (Little-Endian) --- */

static void write_u16_le(unsigned char *buf, uint16_t val) {
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
}

static uint16_t read_u16_le(const unsigned char *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static void write_u32_le(unsigned char *buf, uint32_t val) {
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
    buf[2] = (unsigned char)((val >> 16) & 0xFF);
    buf[3] = (unsigned char)((val >> 24) & 0xFF);
}

static uint32_t read_u32_le(const unsigned char *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static void write_u64_le(unsigned char *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (unsigned char)((val >> (i * 8)) & 0xFF);
    }
}

static uint64_t read_u64_le(const unsigned char *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t)buf[i]) << (i * 8);
    }
    return val;
}

static uint64_t get_file_size_from_path(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
#if defined(_WIN32)
    int fd = _fileno(fp);
    int64_t sz = _filelengthi64(fd);
    fclose(fp);
    return (sz < 0) ? 0 : (uint64_t)sz;
#else
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    return (sz < 0) ? 0 : (uint64_t)sz;
#endif
}

/* --- Min-Heap Priority Queue --- */

typedef struct {
    HuffmanNode **nodes;
    int size;
    int capacity;
} MinHeap;

static MinHeap* minheap_create(int capacity) {
    MinHeap *h = (MinHeap*)malloc(sizeof(MinHeap));
    if (!h) return NULL;
    h->nodes = (HuffmanNode**)malloc(sizeof(HuffmanNode*) * capacity);
    if (!h->nodes) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = capacity;
    return h;
}

static void minheap_destroy(MinHeap *h) {
    if (h) {
        if (h->nodes) free(h->nodes);
        free(h);
    }
}

static void minheap_insert(MinHeap *h, HuffmanNode *node) {
    int i = h->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->nodes[parent]->freq <= node->freq) {
            break;
        }
        h->nodes[i] = h->nodes[parent];
        i = parent;
    }
    h->nodes[i] = node;
}

static HuffmanNode* minheap_extract_min(MinHeap *h) {
    if (h->size == 0) return NULL;
    HuffmanNode *min_node = h->nodes[0];
    HuffmanNode *last_node = h->nodes[--h->size];
    if (h->size > 0) {
        int i = 0;
        while (i * 2 + 1 < h->size) {
            int left = i * 2 + 1;
            int right = i * 2 + 2;
            int smallest = left;
            if (right < h->size && h->nodes[right]->freq < h->nodes[left]->freq) {
                smallest = right;
            }
            if (last_node->freq <= h->nodes[smallest]->freq) {
                break;
            }
            h->nodes[i] = h->nodes[smallest];
            i = smallest;
        }
        h->nodes[i] = last_node;
    }
    return min_node;
}

/* --- Tree Management --- */

static void free_tree(HuffmanNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

static HuffmanNode* build_tree_from_frequencies(const uint64_t freq[256]) {
    MinHeap *heap = minheap_create(512);
    if (!heap) return NULL;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
            if (!node) {
                while (heap->size > 0) {
                    free(minheap_extract_min(heap));
                }
                minheap_destroy(heap);
                return NULL;
            }
            node->symbol = (unsigned char)i;
            node->freq = freq[i];
            node->left = NULL;
            node->right = NULL;
            minheap_insert(heap, node);
        }
    }

    while (heap->size > 1) {
        HuffmanNode *left = minheap_extract_min(heap);
        HuffmanNode *right = minheap_extract_min(heap);
        HuffmanNode *parent = (HuffmanNode*)malloc(sizeof(HuffmanNode));
        if (!parent) {
            free_tree(left);
            free_tree(right);
            while (heap->size > 0) {
                free_tree(minheap_extract_min(heap));
            }
            minheap_destroy(heap);
            return NULL;
        }
        parent->symbol = 0;
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        minheap_insert(heap, parent);
    }

    HuffmanNode *root = minheap_extract_min(heap);
    minheap_destroy(heap);
    return root;
}

static void generate_codes_recursive(HuffmanNode *root, char *current_code, int depth, HuffmanCode *codes) {
    if (!root) return;
    if (root->left == NULL && root->right == NULL) {
        current_code[depth] = '\0';
        memcpy(codes[root->symbol].bits, current_code, depth + 1);
        codes[root->symbol].len = depth;
        return;
    }
    if (root->left) {
        current_code[depth] = '0';
        generate_codes_recursive(root->left, current_code, depth + 1, codes);
    }
    if (root->right) {
        current_code[depth] = '1';
        generate_codes_recursive(root->right, current_code, depth + 1, codes);
    }
}

static void serialize_tree(HuffmanNode *root, BitWriter *bw) {
    if (!root) return;
    if (root->left == NULL && root->right == NULL) {
        bitwriter_write_bit(bw, 1);
        bitwriter_write_byte(bw, root->symbol);
    } else {
        bitwriter_write_bit(bw, 0);
        serialize_tree(root->left, bw);
        serialize_tree(root->right, bw);
    }
}

static HuffmanNode* deserialize_tree(BitReader *br) {
    int bit = bitreader_read_bit(br);
    if (bit == -1) return NULL;
    if (bit == 1) {
        unsigned char sym;
        if (!bitreader_read_byte(br, &sym)) return NULL;
        HuffmanNode *leaf = (HuffmanNode*)malloc(sizeof(HuffmanNode));
        if (!leaf) return NULL;
        leaf->symbol = sym;
        leaf->freq = 0;
        leaf->left = NULL;
        leaf->right = NULL;
        return leaf;
    } else {
        HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
        if (!node) return NULL;
        node->symbol = 0;
        node->freq = 0;
        node->left = deserialize_tree(br);
        node->right = deserialize_tree(br);
        if (!node->left || !node->right) {
            free_tree(node);
            return NULL;
        }
        return node;
    }
}

/* --- Public Error String Function --- */

const char* huffman_result_to_string(HuffmanResult res) {
    switch (res) {
        case HUFF_SUCCESS:
            return "Operation completed successfully";
        case HUFF_ERROR_FILE_OPEN:
            return "Failed to open file (check file path or access permissions)";
        case HUFF_ERROR_FILE_READ:
            return "Error reading from input file";
        case HUFF_ERROR_FILE_WRITE:
            return "Error writing to output file";
        case HUFF_ERROR_INVALID_FORMAT:
            return "Invalid file format (header signature or version mismatch)";
        case HUFF_ERROR_CORRUPTED_DATA:
            return "Compressed bitstream is truncated or corrupted";
        case HUFF_ERROR_CRC_MISMATCH:
            return "Integrity check failed: CRC32 checksum mismatch";
        case HUFF_ERROR_OUT_OF_MEMORY:
            return "Memory allocation failure";
        default:
            return "Unknown error";
    }
}

/* --- Compression --- */

HuffmanResult huffman_compress_file(const char *input_path, const char *output_path, HuffmanStats *stats) {
    clock_t start_time = clock();

    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        return HUFF_ERROR_FILE_OPEN;
    }

    uint64_t freq[256] = {0};
    uint64_t original_size = 0;
    uint32_t original_crc = 0xFFFFFFFFU;

    unsigned char buffer[BITIO_BUFFER_SIZE];
    size_t bytes_read;

    // Pass 1: Frequency analysis and CRC32 calculation
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freq[buffer[i]]++;
        }
        original_crc = crc32_update(original_crc, buffer, bytes_read);
        original_size += bytes_read;
    }

    if (ferror(fin)) {
        fclose(fin);
        return HUFF_ERROR_FILE_READ;
    }
    original_crc = crc32_finalize(original_crc);

    // Count unique symbols
    int unique_count = 0;
    int last_symbol = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            unique_count++;
            last_symbol = i;
        }
    }

    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        fclose(fin);
        return HUFF_ERROR_FILE_OPEN;
    }

    // Write header:
    // [0..3]: Magic 'H','U','F','1'
    // [4]: Version (1)
    // [5]: Flags (0)
    // [6..13]: Original size (uint64_t LE)
    // [14..17]: Original CRC32 (uint32_t LE)
    // [18..19]: Unique symbols count (uint16_t LE)
    // [20]: Single symbol byte (if unique_count == 1)
    unsigned char header[21];
    header[0] = HUFF_MAGIC_0;
    header[1] = HUFF_MAGIC_1;
    header[2] = HUFF_MAGIC_2;
    header[3] = HUFF_MAGIC_3;
    header[4] = HUFF_VERSION;
    header[5] = 0; // Flags
    write_u64_le(&header[6], original_size);
    write_u32_le(&header[14], original_crc);
    write_u16_le(&header[18], (uint16_t)unique_count);

    size_t header_len = 20;
    if (unique_count == 1) {
        header[20] = (unsigned char)last_symbol;
        header_len = 21;
    }

    if (fwrite(header, 1, header_len, fout) != header_len) {
        fclose(fin);
        fclose(fout);
        return HUFF_ERROR_FILE_WRITE;
    }

    // Special cases: 0 bytes or 1 unique symbol
    if (original_size == 0 || unique_count <= 1) {
        fclose(fin);
        fclose(fout);

        clock_t end_time = clock();
        if (stats) {
            stats->original_size = original_size;
            stats->compressed_size = get_file_size_from_path(output_path);
            stats->original_crc = original_crc;
            stats->decompressed_crc = original_crc;
            stats->unique_symbols = unique_count;
            if (original_size > 0) {
                stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
            } else {
                stats->compression_ratio = 0.0;
            }
            stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
        }
        return HUFF_SUCCESS;
    }

    // General case: unique_count >= 2
    HuffmanNode *root = build_tree_from_frequencies(freq);
    if (!root) {
        fclose(fin);
        fclose(fout);
        return HUFF_ERROR_OUT_OF_MEMORY;
    }

    HuffmanCode codes[256];
    memset(codes, 0, sizeof(codes));
    char current_code[MAX_CODE_BITS];
    generate_codes_recursive(root, current_code, 0, codes);

    BitWriter bw;
    bitwriter_init(&bw, fout);

    // 1. Serialize the Huffman tree
    serialize_tree(root, &bw);

    // 2. Rewind input file for Pass 2 (Encoding data)
    rewind(fin);

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char b = buffer[i];
            const HuffmanCode *code = &codes[b];
            for (int j = 0; j < code->len; j++) {
                bitwriter_write_bit(&bw, code->bits[j] - '0');
            }
        }
    }

    bitwriter_flush(&bw);
    free_tree(root);
    fclose(fin);
    fclose(fout);

    clock_t end_time = clock();
    if (stats) {
        stats->original_size = original_size;
        stats->compressed_size = get_file_size_from_path(output_path);
        stats->original_crc = original_crc;
        stats->decompressed_crc = original_crc;
        stats->unique_symbols = unique_count;
        if (original_size > 0) {
            stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
        } else {
            stats->compression_ratio = 0.0;
        }
        stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
    }

    return HUFF_SUCCESS;
}

/* --- Decompression --- */

HuffmanResult huffman_decompress_file(const char *input_path, const char *output_path, HuffmanStats *stats) {
    clock_t start_time = clock();

    FILE *fin = fopen(input_path, "rb");
    if (!fin) {
        return HUFF_ERROR_FILE_OPEN;
    }

    unsigned char header[20];
    if (fread(header, 1, 20, fin) != 20) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    if (header[0] != HUFF_MAGIC_0 || header[1] != HUFF_MAGIC_1 ||
        header[2] != HUFF_MAGIC_2 || header[3] != HUFF_MAGIC_3) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    if (header[4] != HUFF_VERSION) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    uint64_t original_size = read_u64_le(&header[6]);
    uint32_t expected_crc = read_u32_le(&header[14]);
    uint16_t unique_count = read_u16_le(&header[18]);

    if (unique_count > 256) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        fclose(fin);
        return HUFF_ERROR_FILE_OPEN;
    }

    uint32_t current_crc = 0xFFFFFFFFU;

    // Special case 1: Empty file
    if (original_size == 0) {
        fclose(fin);
        fclose(fout);
        current_crc = crc32_finalize(current_crc);
        if (current_crc != expected_crc) {
            return HUFF_ERROR_CRC_MISMATCH;
        }
        clock_t end_time = clock();
        if (stats) {
            stats->original_size = original_size;
            stats->compressed_size = get_file_size_from_path(input_path);
            stats->original_crc = expected_crc;
            stats->decompressed_crc = current_crc;
            stats->unique_symbols = unique_count;
            stats->compression_ratio = 0.0;
            stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
        }
        return HUFF_SUCCESS;
    }

    // Special case 2: Exactly 1 unique symbol
    if (unique_count == 1) {
        int c = fgetc(fin);
        if (c == EOF) {
            fclose(fin);
            fclose(fout);
            return HUFF_ERROR_CORRUPTED_DATA;
        }
        unsigned char sym = (unsigned char)c;
        unsigned char chunk[BITIO_BUFFER_SIZE];
        memset(chunk, sym, sizeof(chunk));

        uint64_t remaining = original_size;
        while (remaining > 0) {
            size_t to_write = (remaining > sizeof(chunk)) ? sizeof(chunk) : (size_t)remaining;
            if (fwrite(chunk, 1, to_write, fout) != to_write) {
                fclose(fin);
                fclose(fout);
                return HUFF_ERROR_FILE_WRITE;
            }
            current_crc = crc32_update(current_crc, chunk, to_write);
            remaining -= to_write;
        }

        current_crc = crc32_finalize(current_crc);
        fclose(fin);
        fclose(fout);

        if (current_crc != expected_crc) {
            return HUFF_ERROR_CRC_MISMATCH;
        }

        clock_t end_time = clock();
        if (stats) {
            stats->original_size = original_size;
            stats->compressed_size = get_file_size_from_path(input_path);
            stats->original_crc = expected_crc;
            stats->decompressed_crc = current_crc;
            stats->unique_symbols = unique_count;
            stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
            stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
        }
        return HUFF_SUCCESS;
    }

    // General case: unique_count >= 2
    BitReader br;
    bitreader_init(&br, fin);

    HuffmanNode *root = deserialize_tree(&br);
    if (!root) {
        fclose(fin);
        fclose(fout);
        return HUFF_ERROR_CORRUPTED_DATA;
    }

    unsigned char out_buf[BITIO_BUFFER_SIZE];
    size_t out_pos = 0;
    uint64_t decoded = 0;
    HuffmanNode *curr = root;

    while (decoded < original_size) {
        int bit = bitreader_read_bit(&br);
        if (bit == -1) {
            free_tree(root);
            fclose(fin);
            fclose(fout);
            return HUFF_ERROR_CORRUPTED_DATA;
        }

        curr = (bit == 1) ? curr->right : curr->left;
        if (!curr) {
            free_tree(root);
            fclose(fin);
            fclose(fout);
            return HUFF_ERROR_CORRUPTED_DATA;
        }

        if (curr->left == NULL && curr->right == NULL) {
            out_buf[out_pos++] = curr->symbol;
            if (out_pos == sizeof(out_buf)) {
                if (fwrite(out_buf, 1, sizeof(out_buf), fout) != sizeof(out_buf)) {
                    free_tree(root);
                    fclose(fin);
                    fclose(fout);
                    return HUFF_ERROR_FILE_WRITE;
                }
                current_crc = crc32_update(current_crc, out_buf, sizeof(out_buf));
                out_pos = 0;
            }
            decoded++;
            curr = root;
        }
    }

    if (out_pos > 0) {
        if (fwrite(out_buf, 1, out_pos, fout) != out_pos) {
            free_tree(root);
            fclose(fin);
            fclose(fout);
            return HUFF_ERROR_FILE_WRITE;
        }
        current_crc = crc32_update(current_crc, out_buf, out_pos);
    }

    current_crc = crc32_finalize(current_crc);
    free_tree(root);
    fclose(fin);
    fclose(fout);

    if (current_crc != expected_crc) {
        return HUFF_ERROR_CRC_MISMATCH;
    }

    clock_t end_time = clock();
    if (stats) {
        stats->original_size = original_size;
        stats->compressed_size = get_file_size_from_path(input_path);
        stats->original_crc = expected_crc;
        stats->decompressed_crc = current_crc;
        stats->unique_symbols = unique_count;
        stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
        stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
    }

    return HUFF_SUCCESS;
}

/* --- Integrity Verification (Without writing output file) --- */

HuffmanResult huffman_verify_file(const char *compressed_path, HuffmanStats *stats) {
    clock_t start_time = clock();

    FILE *fin = fopen(compressed_path, "rb");
    if (!fin) {
        return HUFF_ERROR_FILE_OPEN;
    }

    unsigned char header[20];
    if (fread(header, 1, 20, fin) != 20) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    if (header[0] != HUFF_MAGIC_0 || header[1] != HUFF_MAGIC_1 ||
        header[2] != HUFF_MAGIC_2 || header[3] != HUFF_MAGIC_3) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    if (header[4] != HUFF_VERSION) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    uint64_t original_size = read_u64_le(&header[6]);
    uint32_t expected_crc = read_u32_le(&header[14]);
    uint16_t unique_count = read_u16_le(&header[18]);

    if (unique_count > 256) {
        fclose(fin);
        return HUFF_ERROR_INVALID_FORMAT;
    }

    uint32_t current_crc = 0xFFFFFFFFU;

    if (original_size == 0) {
        fclose(fin);
        current_crc = crc32_finalize(current_crc);
        if (current_crc != expected_crc) {
            return HUFF_ERROR_CRC_MISMATCH;
        }
        clock_t end_time = clock();
        if (stats) {
            stats->original_size = original_size;
            stats->compressed_size = get_file_size_from_path(compressed_path);
            stats->original_crc = expected_crc;
            stats->decompressed_crc = current_crc;
            stats->unique_symbols = unique_count;
            stats->compression_ratio = 0.0;
            stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
        }
        return HUFF_SUCCESS;
    }

    if (unique_count == 1) {
        int c = fgetc(fin);
        if (c == EOF) {
            fclose(fin);
            return HUFF_ERROR_CORRUPTED_DATA;
        }
        unsigned char sym = (unsigned char)c;
        unsigned char chunk[BITIO_BUFFER_SIZE];
        memset(chunk, sym, sizeof(chunk));

        uint64_t remaining = original_size;
        while (remaining > 0) {
            size_t to_proc = (remaining > sizeof(chunk)) ? sizeof(chunk) : (size_t)remaining;
            current_crc = crc32_update(current_crc, chunk, to_proc);
            remaining -= to_proc;
        }
        current_crc = crc32_finalize(current_crc);
        fclose(fin);

        if (current_crc != expected_crc) {
            return HUFF_ERROR_CRC_MISMATCH;
        }
        clock_t end_time = clock();
        if (stats) {
            stats->original_size = original_size;
            stats->compressed_size = get_file_size_from_path(compressed_path);
            stats->original_crc = expected_crc;
            stats->decompressed_crc = current_crc;
            stats->unique_symbols = unique_count;
            stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
            stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
        }
        return HUFF_SUCCESS;
    }

    BitReader br;
    bitreader_init(&br, fin);

    HuffmanNode *root = deserialize_tree(&br);
    if (!root) {
        fclose(fin);
        return HUFF_ERROR_CORRUPTED_DATA;
    }

    unsigned char verify_buf[BITIO_BUFFER_SIZE];
    size_t buf_pos = 0;
    uint64_t decoded = 0;
    HuffmanNode *curr = root;

    while (decoded < original_size) {
        int bit = bitreader_read_bit(&br);
        if (bit == -1) {
            free_tree(root);
            fclose(fin);
            return HUFF_ERROR_CORRUPTED_DATA;
        }

        curr = (bit == 1) ? curr->right : curr->left;
        if (!curr) {
            free_tree(root);
            fclose(fin);
            return HUFF_ERROR_CORRUPTED_DATA;
        }

        if (curr->left == NULL && curr->right == NULL) {
            verify_buf[buf_pos++] = curr->symbol;
            if (buf_pos == sizeof(verify_buf)) {
                current_crc = crc32_update(current_crc, verify_buf, sizeof(verify_buf));
                buf_pos = 0;
            }
            decoded++;
            curr = root;
        }
    }

    if (buf_pos > 0) {
        current_crc = crc32_update(current_crc, verify_buf, buf_pos);
    }

    current_crc = crc32_finalize(current_crc);
    free_tree(root);
    fclose(fin);

    if (current_crc != expected_crc) {
        return HUFF_ERROR_CRC_MISMATCH;
    }

    clock_t end_time = clock();
    if (stats) {
        stats->original_size = original_size;
        stats->compressed_size = get_file_size_from_path(compressed_path);
        stats->original_crc = expected_crc;
        stats->decompressed_crc = current_crc;
        stats->unique_symbols = unique_count;
        stats->compression_ratio = (1.0 - ((double)stats->compressed_size / (double)original_size)) * 100.0;
        stats->elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
    }

    return HUFF_SUCCESS;
}
