#include "ff.h"
#include <stddef.h>
#include <stdlib.h>

// Returns bytes read, or 0 on failure.
// Caller must free() the returned buffer.
uint8_t* read_jpeg_from_sd(const char *path, UINT *out_size) {
    FATFS fs;
    FIL file;
    FRESULT res;
    uint8_t *buf = NULL;
    UINT bytes_read = 0;

    // Mount the filesystem
    res = f_mount(&fs, "", 1);
    if (res != FR_OK) return NULL;

    // Open the file
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) goto cleanup;

    FSIZE_t file_size = f_size(&file);
    buf = malloc(file_size);
    if (!buf) goto close;

    // Read the entire file into memory
    res = f_read(&file, buf, file_size, &bytes_read);
    if (res != FR_OK || bytes_read != file_size) {
        free(buf);
        buf = NULL;
    } else {
        *out_size = bytes_read;
    }

close:
    f_close(&file);
cleanup:
    f_unmount("");
    return buf;
}

// Example usage:
void example(void) {
    UINT jpeg_size = 0;
    uint8_t *jpeg_data = read_jpeg_from_sd("/IMAGE.JPG", &jpeg_size);
    if (jpeg_data) {
        // Pass jpeg_data to your display or decoder
        free(jpeg_data);
    }
}