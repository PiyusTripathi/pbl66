#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "huffman.h"
#include "crc32.h"

static int compare_files(const char *path1, const char *path2) {
    FILE *f1 = fopen(path1, "rb");
    FILE *f2 = fopen(path2, "rb");
    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return 0;
    }

    unsigned char buf1[4096];
    unsigned char buf2[4096];
    size_t n1, n2;

    while (1) {
        n1 = fread(buf1, 1, sizeof(buf1), f1);
        n2 = fread(buf2, 1, sizeof(buf2), f2);
        if (n1 != n2) {
            fclose(f1);
            fclose(f2);
            return 0;
        }
        if (n1 == 0) break;
        if (memcmp(buf1, buf2, n1) != 0) {
            fclose(f1);
            fclose(f2);
            return 0;
        }
    }

    fclose(f1);
    fclose(f2);
    return 1;
}

static void create_file(const char *path, const void *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    if (fp) {
        if (size > 0 && data != NULL) {
            fwrite(data, 1, size, fp);
        }
        fclose(fp);
    }
}

static int run_test(const char *test_name, const char *orig_file, const char *huf_file, const char *restored_file) {
    printf("[TEST] %-30s ... ", test_name);

    HuffmanStats comp_stats, decomp_stats;
    memset(&comp_stats, 0, sizeof(comp_stats));
    memset(&decomp_stats, 0, sizeof(decomp_stats));

    HuffmanResult res = huffman_compress_file(orig_file, huf_file, &comp_stats);
    if (res != HUFF_SUCCESS) {
        printf("FAILED (Compress error: %s)\n", huffman_result_to_string(res));
        return 0;
    }

    res = huffman_verify_file(huf_file, &decomp_stats);
    if (res != HUFF_SUCCESS) {
        printf("FAILED (Verify error: %s)\n", huffman_result_to_string(res));
        return 0;
    }

    res = huffman_decompress_file(huf_file, restored_file, &decomp_stats);
    if (res != HUFF_SUCCESS) {
        printf("FAILED (Decompress error: %s)\n", huffman_result_to_string(res));
        return 0;
    }

    if (!compare_files(orig_file, restored_file)) {
        printf("FAILED (File content mismatch!)\n");
        return 0;
    }

    printf("PASSED (Orig: %llu B -> Comp: %llu B, Saved: %.1f%%)\n",
           (unsigned long long)comp_stats.original_size,
           (unsigned long long)comp_stats.compressed_size,
           comp_stats.compression_ratio);
    return 1;
}

int main(void) {
    printf("==============================================================\n");
    printf("           RUNNING COMPREHENSIVE HUFFMAN TEST SUITE           \n");
    printf("==============================================================\n\n");

    int passed = 0;
    int total = 0;

    // Test 1: Empty File (0 bytes)
    total++;
    create_file("tests/empty.txt", NULL, 0);
    if (run_test("1. Empty File (0 bytes)", "tests/empty.txt", "tests/empty.huf", "tests/empty.restored")) passed++;

    // Test 2: Single Character (1 byte)
    total++;
    create_file("tests/single_byte.txt", "X", 1);
    if (run_test("2. Single Byte ('X')", "tests/single_byte.txt", "tests/single_byte.huf", "tests/single_byte.restored")) passed++;

    // Test 3: Repeated Single Character (50,000 bytes)
    total++;
    char *repeated = (char*)malloc(50000);
    memset(repeated, 'A', 50000);
    create_file("tests/repeated_char.txt", repeated, 50000);
    free(repeated);
    if (run_test("3. Repeated Char (50 KB 'A')", "tests/repeated_char.txt", "tests/repeated_char.huf", "tests/repeated_char.restored")) passed++;

    // Test 4: Two Symbols Alternating
    total++;
    char *two_syms = (char*)malloc(20000);
    for (int i = 0; i < 20000; i++) two_syms[i] = (i % 2 == 0) ? '0' : '1';
    create_file("tests/two_syms.txt", two_syms, 20000);
    free(two_syms);
    if (run_test("4. Two Alternating ('0'/'1')", "tests/two_syms.txt", "tests/two_syms.huf", "tests/two_syms.restored")) passed++;

    // Test 5: English Text Document
    total++;
    const char *sample_text =
        "Huffman coding is a particular type of optimal prefix code that is commonly used for lossless data compression.\n"
        "The process of finding or using such a code proceeds by means of Huffman coding, an algorithm developed by David A. Huffman.\n"
        "The output from Huffman's algorithm can be viewed as a variable-length code table for encoding a source symbol.\n"
        "The algorithm derives this table from the estimated probability or frequency of occurrence for each possible value of the source symbol.\n"
        "As in other entropy encoding methods, more common symbols are generally represented using fewer bits than less common symbols.\n"
        "Huffman's method can be efficiently implemented, finding a code in time linear to the number of input weights if these weights are sorted.\n"
        "However, although optimal among methods encoding symbols separately, Huffman coding is not always optimal among all compression methods;\n"
        "it is replaced with arithmetic coding or asymmetric numeral systems if better compression ratio is required.\n";
    create_file("tests/sample_text.txt", sample_text, strlen(sample_text));
    if (run_test("5. English Text Article", "tests/sample_text.txt", "tests/sample_text.huf", "tests/sample_text.restored")) passed++;

    // Test 6: All 256 Byte Values (Binary full range 0x00 to 0xFF)
    total++;
    unsigned char all_bytes[256 * 100];
    for (int i = 0; i < 256 * 100; i++) {
        all_bytes[i] = (unsigned char)(i % 256);
    }
    create_file("tests/all_bytes.bin", all_bytes, sizeof(all_bytes));
    if (run_test("6. Binary All Bytes (0..255)", "tests/all_bytes.bin", "tests/all_bytes.huf", "tests/all_bytes.restored")) passed++;

    // Test 7: Skewed Distribution (Zipf-like realistic frequency)
    total++;
    size_t skewed_size = 100000;
    unsigned char *skewed = (unsigned char*)malloc(skewed_size);
    for (size_t i = 0; i < skewed_size; i++) {
        int r = rand() % 100;
        if (r < 50) skewed[i] = 'e';
        else if (r < 75) skewed[i] = 't';
        else if (r < 85) skewed[i] = 'a';
        else if (r < 92) skewed[i] = 'o';
        else skewed[i] = (unsigned char)(rand() % 256);
    }
    create_file("tests/skewed.bin", skewed, skewed_size);
    free(skewed);
    if (run_test("7. Skewed Distribution (100KB)", "tests/skewed.bin", "tests/skewed.huf", "tests/skewed.restored")) passed++;

    // Test 8: Huffman Executable itself (Binary Compression)
    total++;
    if (run_test("8. Executable Binary (huffman.exe)", "huffman.exe", "tests/huffman_exe.huf", "tests/huffman_exe.restored")) passed++;

    // Test 9: Corruption Detection Test
    total++;
    printf("[TEST] %-30s ... ", "9. Corrupted File Detection");
    FILE *corrupt_fp = fopen("tests/skewed.huf", "r+b");
    if (corrupt_fp) {
        fseek(corrupt_fp, 40, SEEK_SET); // Corrupt bitstream inside file
        int byte = fgetc(corrupt_fp);
        if (byte != EOF) {
            fseek(corrupt_fp, 40, SEEK_SET);
            fputc(byte ^ 0xFF, corrupt_fp); // flip all 8 bits
        }
        fclose(corrupt_fp);

        HuffmanStats corrupt_stats;
        HuffmanResult res_verify = huffman_verify_file("tests/skewed.huf", &corrupt_stats);
        if (res_verify == HUFF_ERROR_CRC_MISMATCH || res_verify == HUFF_ERROR_CORRUPTED_DATA) {
            printf("PASSED (Correctly detected corruption: %s)\n", huffman_result_to_string(res_verify));
            passed++;
        } else {
            printf("FAILED (Expected corruption error, got: %s)\n", huffman_result_to_string(res_verify));
        }
    } else {
        printf("FAILED (Could not open file for corruption)\n");
    }

    printf("\n==============================================================\n");
    printf("                  TEST RESULTS: %d / %d PASSED                \n", passed, total);
    printf("==============================================================\n");

    return (passed == total) ? 0 : 1;
}
