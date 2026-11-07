#pragma once

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
// File IO
FILE* safe_fopen(const char* name, const char* mode);

bool safe_fclose(FILE* fdptr, const char* name);