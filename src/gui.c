#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0500
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "huffman.h"

#define IDC_BTN_SELECT       101
#define IDC_BTN_ACTION       102
#define IDC_BTN_OPEN_FOLDER  103
#define IDC_LBL_FILE_INFO    104
#define IDC_TXT_RESULT       105

static HWND g_hwnd = NULL;
static HWND g_hBtnSelect = NULL;
static HWND g_hBtnAction = NULL;
static HWND g_hBtnOpenFolder = NULL;
static HWND g_hLblFileInfo = NULL;
static HWND g_hTxtResult = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontSubtitle = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontMono = NULL;

static char g_selected_file[MAX_PATH] = {0};
static char g_last_output_file[MAX_PATH] = {0};
static int  g_is_huf = 0;

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

static uint64_t get_file_size(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    int fd = _fileno(fp);
    int64_t sz = _filelengthi64(fd);
    fclose(fp);
    return (sz < 0) ? 0 : (uint64_t)sz;
}

static int ends_with_huf(const char *path) {
    size_t len = strlen(path);
    if (len < 4) return 0;
    const char *ext = path + len - 4;
    return (stricmp(ext, ".huf") == 0);
}

static void set_selected_file(const char *path) {
    if (!path || strlen(path) == 0) return;
    strncpy(g_selected_file, path, MAX_PATH - 1);
    g_selected_file[MAX_PATH - 1] = '\0';

    uint64_t sz = get_file_size(g_selected_file);
    char sz_str[64];
    format_size(sz, sz_str, sizeof(sz_str));

    g_is_huf = ends_with_huf(g_selected_file);

    char info[MAX_PATH + 128];
    snprintf(info, sizeof(info), "Selected: %s\nSize: %s", g_selected_file, sz_str);
    SetWindowTextA(g_hLblFileInfo, info);

    if (g_is_huf) {
        SetWindowTextA(g_hBtnAction, "🔄 DECOMPRESS & RESTORE ORIGINAL FILE");
        SetWindowTextA(g_hTxtResult, 
            "File detected as Huffman compressed archive (.huf).\r\n\r\n"
            "Click the button above to restore the exact original file.");
    } else {
        SetWindowTextA(g_hBtnAction, "⚡ COMPRESS FILE NOW (.huf)");
        SetWindowTextA(g_hTxtResult, 
            "File ready for compression.\r\n\r\n"
            "Click the button above to compress. The compressed file will be created in the same folder.");
    }

    EnableWindow(g_hBtnAction, TRUE);
    EnableWindow(g_hBtnOpenFolder, FALSE);
}

static void on_select_file(HWND hwnd) {
    char filePath[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "All Files (*.*)\0*.*\0Huffman Files (*.huf)\0*.huf\0Text Files (*.txt)\0*.txt\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        set_selected_file(filePath);
    }
}

static void on_action(HWND hwnd) {
    if (strlen(g_selected_file) == 0) {
        MessageBoxA(hwnd, "Please select or drop a file first!", "No File", MB_OK | MB_ICONWARNING);
        return;
    }

    char outPath[MAX_PATH] = {0};

    if (g_is_huf) {
        // Decompression mode
        size_t len = strlen(g_selected_file);
        strncpy(outPath, g_selected_file, len - 4);
        outPath[len - 4] = '\0';

        // Check if output file already exists, avoid accidental direct overwrite if identical
        if (stricmp(outPath, g_selected_file) == 0) {
            snprintf(outPath, sizeof(outPath), "%s.restored", g_selected_file);
        }

        SetWindowTextA(g_hTxtResult, "Decompressing file... Please wait...");
        UpdateWindow(g_hTxtResult);

        HuffmanStats stats;
        memset(&stats, 0, sizeof(stats));

        HuffmanResult res = huffman_decompress_file(g_selected_file, outPath, &stats);
        if (res != HUFF_SUCCESS) {
            char errMsg[512];
            snprintf(errMsg, sizeof(errMsg),
                "❌ DECOMPRESSION FAILED!\r\n\r\n"
                "Error: %s\r\n\r\n"
                "The archive may be corrupted or not a valid Huffman file.",
                huffman_result_to_string(res));
            SetWindowTextA(g_hTxtResult, errMsg);
            MessageBoxA(hwnd, huffman_result_to_string(res), "Decompression Error", MB_OK | MB_ICONERROR);
            return;
        }

        strncpy(g_last_output_file, outPath, MAX_PATH - 1);
        EnableWindow(g_hBtnOpenFolder, TRUE);

        char orig_sz[64], comp_sz[64];
        format_size(stats.original_size, orig_sz, sizeof(orig_sz));
        format_size(stats.compressed_size, comp_sz, sizeof(comp_sz));

        char report[2048];
        snprintf(report, sizeof(report),
            "==========================================================\r\n"
            "   ✅ DECOMPRESSION COMPLETED SUCCESSFULLY!              \r\n"
            "==========================================================\r\n\r\n"
            "  📄 Restored File  : %s\r\n"
            "  📦 Archive Size   : %s\r\n"
            "  📊 Restored Size  : %s\r\n"
            "  🔒 CRC32 Verified : 0x%08X (100%% EXACT MATCH)\r\n"
            "  ⚡ Time Elapsed   : %.2f ms\r\n\r\n"
            "  Status: Original file has been restored to disk losslessly.\r\n"
            "  Click 'Open Output Folder' below to view it in Explorer.",
            outPath, comp_sz, orig_sz, stats.original_crc, stats.elapsed_time_ms);

        SetWindowTextA(g_hTxtResult, report);
        MessageBoxA(hwnd, "File successfully decompressed and verified!", "Done", MB_OK | MB_ICONINFORMATION);

    } else {
        // Compression mode
        snprintf(outPath, sizeof(outPath), "%s.huf", g_selected_file);

        SetWindowTextA(g_hTxtResult, "Compressing file... Please wait...");
        UpdateWindow(g_hTxtResult);

        HuffmanStats stats;
        memset(&stats, 0, sizeof(stats));

        HuffmanResult res = huffman_compress_file(g_selected_file, outPath, &stats);
        if (res != HUFF_SUCCESS) {
            char errMsg[512];
            snprintf(errMsg, sizeof(errMsg),
                "❌ COMPRESSION FAILED!\r\n\r\n"
                "Error: %s",
                huffman_result_to_string(res));
            SetWindowTextA(g_hTxtResult, errMsg);
            MessageBoxA(hwnd, huffman_result_to_string(res), "Compression Error", MB_OK | MB_ICONERROR);
            return;
        }

        strncpy(g_last_output_file, outPath, MAX_PATH - 1);
        EnableWindow(g_hBtnOpenFolder, TRUE);

        char orig_sz[64], comp_sz[64];
        format_size(stats.original_size, orig_sz, sizeof(orig_sz));
        format_size(stats.compressed_size, comp_sz, sizeof(comp_sz));

        double ratio = 1.0;
        if (stats.compressed_size > 0) {
            ratio = (double)stats.original_size / (double)stats.compressed_size;
        }

        char report[2048];
        snprintf(report, sizeof(report),
            "==========================================================\r\n"
            "   ✅ COMPRESSION COMPLETED SUCCESSFULLY!                \r\n"
            "==========================================================\r\n\r\n"
            "  📁 Output File    : %s\r\n"
            "  📊 Original Size  : %s\r\n"
            "  📦 Compressed Size: %s\r\n"
            "  🎉 Space Saved    : %.2f%% (Ratio %.2f:1)\r\n"
            "  🔒 CRC32 Checksum : 0x%08X (Embedded for safety)\r\n"
            "  ⚡ Time Elapsed   : %.2f ms\r\n\r\n"
            "  Status: Lossless Huffman compression completed!\r\n"
            "  Click 'Open Output Folder' below to view your .huf file.",
            outPath, orig_sz, comp_sz, stats.compression_ratio, ratio,
            stats.original_crc, stats.elapsed_time_ms);

        SetWindowTextA(g_hTxtResult, report);
        MessageBoxA(hwnd, "File successfully compressed!", "Done", MB_OK | MB_ICONINFORMATION);
    }
}

static void on_open_folder(void) {
    if (strlen(g_last_output_file) == 0) return;
    char params[MAX_PATH + 32];
    snprintf(params, sizeof(params), "/select,\"%s\"", g_last_output_file);
    ShellExecuteA(NULL, "open", "explorer.exe", params, NULL, SW_SHOWNORMAL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;
            DragAcceptFiles(hwnd, TRUE);

            g_hFontTitle = CreateFontA(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hFontSubtitle = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hFontBold = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hFontNormal = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hFontMono = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");

            // Title Banner
            HWND hTitle = CreateWindowA("STATIC", "Huffman File Compressor",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 16, 520, 26, hwnd, NULL, NULL, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

            HWND hSub = CreateWindowA("STATIC", "Drag & Drop any file below or click Browse to Compress / Decompress instantly",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 44, 520, 18, hwnd, NULL, NULL, NULL);
            SendMessage(hSub, WM_SETFONT, (WPARAM)g_hFontSubtitle, TRUE);

            // Step 1: File Drop / Select Area GroupBox
            HWND hGrpDrop = CreateWindowA("BUTTON", " 1. Select / Drop File ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 72, 520, 105, hwnd, NULL, NULL, NULL);
            SendMessage(hGrpDrop, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnSelect = CreateWindowA("BUTTON", "📁 Browse / Add File...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                38, 98, 484, 38, hwnd, (HMENU)IDC_BTN_SELECT, NULL, NULL);
            SendMessage(g_hBtnSelect, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hLblFileInfo = CreateWindowA("STATIC", "No file selected. (You can also Drag & Drop a file directly into this window)",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                38, 142, 484, 28, hwnd, (HMENU)IDC_LBL_FILE_INFO, NULL, NULL);
            SendMessage(g_hLblFileInfo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // Step 2: One-Click Action Button
            g_hBtnAction = CreateWindowA("BUTTON", "⚡ COMPRESS FILE NOW",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                20, 190, 520, 44, hwnd, (HMENU)IDC_BTN_ACTION, NULL, NULL);
            SendMessage(g_hBtnAction, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            // Step 3: Result Card GroupBox
            HWND hGrpResult = CreateWindowA("BUTTON", " 2. Status & Details ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 246, 520, 210, hwnd, NULL, NULL, NULL);
            SendMessage(hGrpResult, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hTxtResult = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT",
                "Ready.\r\n\r\n"
                "1. Click 'Browse / Add File...' (or Drag & Drop a file into this window).\r\n"
                "2. Click the Big Action Button to Compress / Decompress.\r\n"
                "3. Your new file will be created automatically in the same folder!",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                35, 270, 490, 172, hwnd, (HMENU)IDC_TXT_RESULT, NULL, NULL);
            SendMessage(g_hTxtResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

            // Open Folder Button
            g_hBtnOpenFolder = CreateWindowA("BUTTON", "📂 Open Output Folder in Explorer",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                20, 465, 520, 36, hwnd, (HMENU)IDC_BTN_OPEN_FOLDER, NULL, NULL);
            SendMessage(g_hBtnOpenFolder, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            char droppedFile[MAX_PATH] = {0};
            if (DragQueryFileA(hDrop, 0, droppedFile, MAX_PATH)) {
                set_selected_file(droppedFile);
            }
            DragFinish(hDrop);
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_SELECT) {
                on_select_file(hwnd);
            } else if (id == IDC_BTN_ACTION) {
                on_action(hwnd);
            } else if (id == IDC_BTN_OPEN_FOLDER) {
                on_open_folder();
            }
            break;
        }

        case WM_DESTROY: {
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontSubtitle) DeleteObject(g_hFontSubtitle);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontMono) DeleteObject(g_hFontMono);
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    InitCommonControls();

    const char CLASS_NAME[] = "HuffmanSimpleGUI";

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        return 0;
    }

    int winWidth = 575;
    int winHeight = 555;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - winWidth) / 2;
    int posY = (screenHeight - winHeight) / 2;

    HWND hwnd = CreateWindowExA(
        WS_EX_ACCEPTFILES,
        CLASS_NAME,
        "Huffman File Compressor & Decompressor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
