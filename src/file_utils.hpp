#ifndef __my_file_utils__

#include <stdlib.h>
#include <stdio.h>

// ===== header =====
int fgets_str(char* buf, int max_length, const char* text, long* text_index);
const char* mallocTextFromFile(const char* filename);
// ===== end header =====

// for optimized .obj loading
int fgets_str(char* buf, int max_length, const char* text, long* text_index) {
    if (text[*text_index] == '\0') { buf[0] = '\0'; return -1; }
    int buffer_index = 0;
    bool shouldEnd = false;
    char c;
    while (buffer_index < max_length - 1 && !shouldEnd) {
        c = text[(*text_index)++];
        if (c == '\n') {
            shouldEnd = true;
        } else if (c == '\0') {
            (*text_index)--;
            shouldEnd = true;
        } else {
            buf[buffer_index++] = c;
        }
    }
    buf[buffer_index] = '\0';
    return buffer_index;
}

// reads text file onto a new string
const char* mallocTextFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) { fprintf(stderr, "Failed to open text file: %s\n", filename); return NULL; }

    // Seek to end to get file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    // Allocate buffer (+1 for null terminator)
    char* text = (char*)malloc(sizeof(char) * (length + 1));
    if (!text) { fclose(file); fprintf(stderr, "Memory allocation failed in mallocTextFromFile\n"); return NULL; }

    // Read file into buffer
    size_t read_size = fread(text, 1, length, file);
    text[read_size] = '\0'; // Null-terminate

    fclose(file);
    return text;
}

#define __my_file_utils__
#endif