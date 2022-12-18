#pragma once

#define bool int
#define true 1
#define false 0

#define UNUSED(x) ((void)x)

typedef struct Descriptors {
    int in;
    int out;
    int err;
} Descriptors;

char* lsh_allocate_from_slice(char const* begin, char const* end);
void* lsh_alloc_and_zero(unsigned int size);

// lsh_getline
// Read a single line of input from stdin.
//
// Parameters:
// buffer - the buffer that will be created by getline. *buffer will be
//          overwritten by lsh_getline with the address of a new buffer.
//
// Returns:
// The number of characters read or -1 on EOF.
//
int lsh_getline(char** buffer);
