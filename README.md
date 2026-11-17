# Huffman File Compression & Decompression Tool (C Language)

Lossless binary file compression tool implemented from scratch in pure C using **Huffman Coding**, **Min-Heap Priority Queue**, **Bit-level Buffered I/O**, and **CRC32 Checksum Integrity Verification**.

---

## 🌟 Key Features

1. **Lossless Binary Compression**:
   - Supports **all file types** (`.txt`, `.bin`, `.exe`, `.pdf`, `.jpg`, `.mp3`, etc.).
   - Full byte range support: Handles all 256 byte values (`0x00` to `0xFF`).
2. **Deterministic Pre-Order Tree Serialization**:
   - Stores the Huffman tree compactly in the header using bit-level pre-order traversal (1 bit per internal node, 9 bits per leaf).
   - Maximum tree overhead is only ~320 bytes.
3. **CRC32 Data Integrity Verification**:
   - Embeds standard IEEE 802.3 CRC32 checksum in the header.
   - Verifies the decompressed data matches the original byte-for-byte upon restoration.
4. **Buffered Bit-Level I/O (64 KB Chunks)**:
   - High throughput streaming: Fast bit manipulation buffered into 64 KB memory blocks.
5. **Edge Case Safety**:
   - **0-byte files** (empty files).
   - **1-byte files** and **single-character repeated files** (e.g. 100 MB of 'A' compresses to 21 bytes!).
   - **Corrupted files / truncated files**: Safely detected without crashing or hanging.
6. **Integrity Verification Mode (`-v`)**:
   - Verify archive validity and CRC32 without writing any files to disk.

---

## 📁 Project Structure

```
├── include/
│   ├── bitio.h           # 64KB-buffered BitWriter & BitReader
│   ├── crc32.h           # CRC32 table & checksum calculation
│   └── huffman.h         # Huffman Tree, Codes, Header & API prototypes
├── src/
│   ├── bitio.c           # Bit-level input/output implementation
│   ├── crc32.c           # CRC32 lookup table implementation
│   ├── huffman.c         # Compression, Decompression & Verification engine
│   └── main.c            # Command-line interface (CLI)
├── tests/
│   ├── test_runner.c     # Automated 9-test verification suite
│   └── sample_text.txt   # Sample test input
├── Makefile              # Makefile for GCC
├── build.bat             # 1-click Windows compilation script
├── huffman.exe           # Compiled executable tool
└── README.md             # Documentation
```

---

## ⚙️ Compilation

### Windows (Command Prompt / PowerShell)
Using `build.bat`:
```cmd
build.bat
```

Or directly via GCC:
```bash
gcc -O3 -Wall -Wextra -D__USE_MINGW_ANSI_STDIO=1 -Iinclude src/main.c src/huffman.c src/bitio.c src/crc32.c -o huffman.exe
```

### Linux / macOS
```bash
make
```

---

## 🚀 How to Use

### 🖥️ Option 1: Native Windows Desktop GUI (Recommended!)
Aap bina terminal open kiye **`huffman_gui.exe`** par double click karke chala sakte hain:
- Native Windows File Dialogs (**Browse...**)
- **Drag & Drop** support: Kisi bhi file ko seedhe window me drag & drop karein.
- **One-Click Actions**:
  - `Compress File (.huf)`
  - `Decompress (.huf)`
  - `Verify Integrity`
- Beautiful detailed statistics & CRC32 verification report.

---

### 💻 Option 2: Command Line (CLI)

#### 1. Compress a file
```bash
huffman.exe -c <input_file> <output_file>
```
**Example:**
```bash
huffman.exe -c document.txt document.huf
```

#### 2. Decompress a file
```bash
huffman.exe -d <compressed_file> <restored_file>
```
**Example:**
```bash
huffman.exe -d document.huf restored_document.txt
```

#### 3. Verify archive integrity
```bash
huffman.exe -v <compressed_file>
```
**Example:**
```bash
huffman.exe -v document.huf
```

---

## 🧪 Automated Testing

An automated test suite (`tests/test_runner.c`) is included to verify all edge cases:
1. Empty File (0 bytes)
2. Single Byte File
3. Repeated Character File (50 KB of 'A')
4. Alternating Bits ('0'/'1')
5. English Text Document
6. Full Range Binary Data (All 256 bytes from `0x00` to `0xFF`)
7. Skewed Distribution (100 KB)
8. Executable Binary (`huffman.exe`)
9. Corrupted File Bit-Flip Detection (CRC32 failure check)

To run tests:
```bash
test_runner.exe
```
