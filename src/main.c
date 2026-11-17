#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "huffman.h"

static void format_size(uint64_t bytes, char *out, size_t out_size) {
    if (bytes < 1024) {
        snprintf(out, out_size, "%" PRIu64 " B", bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        snprintf(out, out_size, "%.2f KB (%" PRIu64 " B)", (double)bytes / 1024.0, bytes);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        snprintf(out, out_size, "%.2f MB (%" PRIu64 " B)", (double)bytes / (1024.0 * 1024.0), bytes);
    } else {
        snprintf(out, out_size, "%.2f GB (%" PRIu64 " B)", (double)bytes / (1024.0 * 1024.0 * 1024.0), bytes);
    }
}

static void print_banner(void) {
    printf("==============================================================\n");
    printf("   HUFFMAN FILE COMPRESSOR & DECOMPRESSOR (C Implementation)  \n");
    printf("   Lossless Binary Compression | CRC32 Integrity Verification  \n");
    printf("==============================================================\n\n");
}

static void print_usage(const char *prog_name) {
    print_banner();
    printf("Usage:\n");
    printf("  %s -c <input_file> <output_file>   : Compress file\n", prog_name);
    printf("  %s -d <input_file> <output_file>   : Decompress file\n", prog_name);
    printf("  %s -v <compressed_file>            : Verify compressed file integrity\n", prog_name);
    printf("  %s -h                              : Display this help message\n\n", prog_name);
    printf("Alternative command syntax:\n");
    printf("  %s compress   <input_file> <output_file>\n", prog_name);
    printf("  %s decompress <input_file> <output_file>\n", prog_name);
    printf("  %s verify     <compressed_file>\n\n", prog_name);
    printf("Examples:\n");
    printf("  %s -c document.txt document.huf\n", prog_name);
    printf("  %s -d document.huf restored.txt\n", prog_name);
    printf("  %s -v document.huf\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *action = argv[1];

    if (strcmp(action, "-h") == 0 || strcmp(action, "--help") == 0 || strcmp(action, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(action, "-c") == 0 || strcmp(action, "compress") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: Compress requires <input_file> and <output_file>.\n\n");
            print_usage(argv[0]);
            return 1;
        }

        const char *input_path = argv[2];
        const char *output_path = argv[3];

        print_banner();
        printf("[*] Compressing: '%s' -> '%s'...\n", input_path, output_path);

        HuffmanStats stats;
        memset(&stats, 0, sizeof(stats));

        HuffmanResult res = huffman_compress_file(input_path, output_path, &stats);
        if (res != HUFF_SUCCESS) {
            fprintf(stderr, "\n[X] Compression failed: %s\n", huffman_result_to_string(res));
            return (int)res;
        }

        char orig_str[64], comp_str[64];
        format_size(stats.original_size, orig_str, sizeof(orig_str));
        format_size(stats.compressed_size, comp_str, sizeof(comp_str));

        printf("\n[+] Compression Succeeded!\n");
        printf("--------------------------------------------------------------\n");
        printf("  Original File Size   : %s\n", orig_str);
        printf("  Compressed File Size : %s\n", comp_str);
        printf("  Unique Byte Symbols  : %d / 256\n", stats.unique_symbols);
        printf("  Space Saved          : %.2f%%\n", stats.compression_ratio);
        if (stats.compressed_size > 0) {
            printf("  Compression Ratio    : %.2f:1\n", (double)stats.original_size / (double)stats.compressed_size);
        }
        printf("  Original CRC32       : 0x%08X\n", stats.original_crc);
        printf("  Processing Time      : %.2f ms\n", stats.elapsed_time_ms);
        printf("--------------------------------------------------------------\n");
        return 0;
    }

    if (strcmp(action, "-d") == 0 || strcmp(action, "decompress") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: Decompress requires <input_file> and <output_file>.\n\n");
            print_usage(argv[0]);
            return 1;
        }

        const char *input_path = argv[2];
        const char *output_path = argv[3];

        print_banner();
        printf("[*] Decompressing: '%s' -> '%s'...\n", input_path, output_path);

        HuffmanStats stats;
        memset(&stats, 0, sizeof(stats));

        HuffmanResult res = huffman_decompress_file(input_path, output_path, &stats);
        if (res != HUFF_SUCCESS) {
            fprintf(stderr, "\n[X] Decompression failed: %s\n", huffman_result_to_string(res));
            return (int)res;
        }

        char orig_str[64], comp_str[64];
        format_size(stats.original_size, orig_str, sizeof(orig_str));
        format_size(stats.compressed_size, comp_str, sizeof(comp_str));

        printf("\n[+] Decompression Succeeded!\n");
        printf("--------------------------------------------------------------\n");
        printf("  Compressed File Size : %s\n", comp_str);
        printf("  Restored File Size   : %s\n", orig_str);
        printf("  Unique Byte Symbols  : %d / 256\n", stats.unique_symbols);
        printf("  Original CRC32       : 0x%08X\n", stats.original_crc);
        printf("  Decompressed CRC32   : 0x%08X [VERIFIED MATCH]\n", stats.decompressed_crc);
        printf("  Processing Time      : %.2f ms\n", stats.elapsed_time_ms);
        printf("--------------------------------------------------------------\n");
        return 0;
    }

    if (strcmp(action, "-v") == 0 || strcmp(action, "-t") == 0 || strcmp(action, "verify") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Error: Verify requires <compressed_file>.\n\n");
            print_usage(argv[0]);
            return 1;
        }

        const char *input_path = argv[2];

        print_banner();
        printf("[*] Verifying integrity of: '%s'...\n", input_path);

        HuffmanStats stats;
        memset(&stats, 0, sizeof(stats));

        HuffmanResult res = huffman_verify_file(input_path, &stats);
        if (res != HUFF_SUCCESS) {
            fprintf(stderr, "\n[X] Verification failed: %s\n", huffman_result_to_string(res));
            return (int)res;
        }

        char orig_str[64], comp_str[64];
        format_size(stats.original_size, orig_str, sizeof(orig_str));
        format_size(stats.compressed_size, comp_str, sizeof(comp_str));

        printf("\n[+] Archive Integrity Verified 100%% Valid!\n");
        printf("--------------------------------------------------------------\n");
        printf("  Compressed File Size : %s\n", comp_str);
        printf("  Uncompressed Size    : %s\n", orig_str);
        printf("  Unique Byte Symbols  : %d / 256\n", stats.unique_symbols);
        printf("  CRC32 Checksum       : 0x%08X [OK]\n", stats.original_crc);
        printf("  Verification Time    : %.2f ms\n", stats.elapsed_time_ms);
        printf("--------------------------------------------------------------\n");
        return 0;
    }

    fprintf(stderr, "Unknown command: '%s'\n\n", action);
    print_usage(argv[0]);
    return 1;
}
