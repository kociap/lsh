#include <common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* lsh_allocate_from_slice(char const* const begin, char const* const end) {
    char* memory = malloc(end - begin);
    if(!memory) {
        fprintf(stderr, "allocate_from_slice: allocation failure");
        exit(EXIT_FAILURE);
    }
    memcpy(memory, begin, end - begin);
    return memory;
}

void* lsh_alloc_and_zero(unsigned int const size) {
    void* memory = malloc(size);
    if(!memory) {
        fprintf(stderr, "alloc_and_zero: allocation failure");
        exit(EXIT_FAILURE);
    }
    memset(memory, 0, size);
    return memory;
}

int lsh_getline(char** out_buffer) {
    int capacity = 0;
    int size = 0;
    char* buffer = NULL;
    while(true) {
        char const c = getchar();
        if(c == EOF) {
            return size == 0 ? -1 : size;
        }

        if(c == '\n') {
            return size;
        }

        if(size + 2 >= capacity) {
            capacity = (capacity == 0 ? 128 : capacity * 2);
            buffer = realloc(buffer, capacity);
            *out_buffer = buffer;
            if(!buffer) {
                fprintf(stderr, "getline: allocation failure");
                exit(EXIT_FAILURE);
            }
        }

        buffer[size] = c;
        buffer[size + 1] = '\0';
        size += 1;
    }
}
