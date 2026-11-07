#include "./safefOC.h"

FILE* safe_fopen(const char* name, const char* mode)
{
    FILE* file_ptr = fopen(name, mode);
    if(file_ptr != NULL)
    {
        return file_ptr;
    }

    fprintf(stderr, "File \"%s\" count not open! (%s)", name, strerror(errno));

    return NULL;

}

bool safe_fclose(FILE* fdptr, const char* name)
{

    if(fdptr == NULL)
    {   
        fprintf(stderr, "File pointer \"%s\": NULL", name);
        return 1;
    }

    if(fclose(fdptr) != 0)
    {
        fprintf(stderr, "File \"%s\" count not open! (%s)", name, strerror(errno));
        fdptr = NULL;
        return 1;
    }

    fdptr = NULL;

    return 0;

}