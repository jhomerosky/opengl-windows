#include <stdlib.h>
#include <stdio.h>

// for optimized .obj loading
static inline void skip_whitespace(char** p) {
    while (**p == ' ' || **p == '\t') ++(*p);
}

// for optimized .obj loading
static inline unsigned int fast_atou(char** p) {
    unsigned int result = 0;
    while (**p >= '0' && **p <= '9') {
        result = result * 10 + (**p - '0');
        ++(*p);
    }
    return result;
}

// for optimized .obj loading
static inline float fast_atof(char** p) {
    float result = 0.0f;
    int sign = 1;

    // Handle optional sign
    if (**p == '-') {
        sign = -1;
        ++(*p);
    } else if (**p == '+') {
        ++(*p);
    }

    // Parse integer part
    while (**p >= '0' && **p <= '9') {
        result = result * 10.0f + (**p - '0');
        ++(*p);
    }

    // Parse fractional part
    if (**p == '.') {
        ++(*p);
        float frac = 0.0f, base = 0.1f;
        while (**p >= '0' && **p <= '9') {
            frac += (**p - '0') * base;
            base *= 0.1f;
            ++(*p);
        }
        result += frac;
    }

    return sign * result;
}

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