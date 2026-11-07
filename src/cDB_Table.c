#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "./cDB_Table.h"

cDB_Table* cDB_Table_init(const uint32_t maxAttribute, const uint32_t maxRecord)
{

    // Allocate the master table container struct on the heap
    cDB_Table* t = (cDB_Table*)malloc(sizeof(cDB_Table));
    if(!t) return NULL;

    t->maxAttribute = maxAttribute;
    t->maxRecord = maxRecord;

    // Allocate trackers
    t->AttributeSizes = (size_t*)calloc(maxAttribute, sizeof(size_t));
    t->data = (void**)calloc(maxAttribute, sizeof(void*));

    // Allocate alias grid buffers (maxAttribute / maxRecord rows of 65 chars)
    t->AttributeAlias = (char(*)[65])calloc(maxAttribute, sizeof(char[65]));
    t->RecordAlias = (char(*)[65])calloc(maxRecord, sizeof(char[65]));

    // Handling Allocations if fails (destroy the table) and return NULL
    if (!t->AttributeSizes || !t->data || !t->AttributeAlias || !t->RecordAlias) {
        cDB_Table_destroy(t);
        return NULL;
    }

    return t;

};

void cDB_Table_destroy(cDB_Table* t)
{
    if (!t) return;

    // Free each individual column's contiguous memory block
    if (t->data) {
        for (uint32_t i = 0; i < t->maxAttribute; i++) {
            // Free whole chunk of Records array if data[AttributeIdx] exist
            if (t->data[i]) {
                free(t->data[i]); 
            }
        }
        free(t->data); // Then free the data** itself
    }

    // Free metadata and alias arrays
    free(t->AttributeSizes);
    free(t->AttributeAlias);
    free(t->RecordAlias);

    // Free the table struct itself
    free(t);
}


uint32_t cDB_Table_getAttributeIdx(cDB_Table* t, const char* alias)
{
    if(!t || !alias) return UINT32_MAX;

    for(uint32_t i = 0; i < t->maxAttribute; i++)
    {
        if(strcmp(t->AttributeAlias[i], alias) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;

};

uint32_t cDB_Table_getRecordIdx(cDB_Table* t, const char* alias)
{
    if(!t || !alias) return UINT32_MAX;

    for(uint32_t i = 0; i < t->maxRecord; i++)
    {
        if(strcmp(t->RecordAlias[i], alias) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
};

void cDB_Table_setRecordAlias(cDB_Table* t, uint32_t RecordIdx, const char* alias)
{
    if(!t || !alias) return;
    if(RecordIdx >= t->maxRecord) return;

    strncpy(t->RecordAlias[RecordIdx], alias, 65);
    t->RecordAlias[RecordIdx][64] = '\0';

};

void cDB_Table_setAttributeAlias(cDB_Table* t, uint32_t AttributeIdx, const char* alias)
{
    if(!t || !alias) return;
    if(AttributeIdx >= t->maxAttribute) return;

    strncpy(t->AttributeAlias[AttributeIdx], alias, 65);
    t->AttributeAlias[AttributeIdx][64] = '\0';
};

void cDB_Table_setAttributeSize(cDB_Table* t, uint32_t AttributeIdx, size_t size)
{
    if(!t) return;
    if(size == 0) return;
    if(AttributeIdx >= t->maxAttribute) return;

    t->AttributeSizes[AttributeIdx] = size;
};


void cDB_Table_setRecord(cDB_Table* t, uint32_t RecordIdx, uint32_t AttributeIdx, void* data)
{
    if(!t || !data) return;
    if(AttributeIdx >= t->maxAttribute) return;
    if(RecordIdx >= t->maxRecord) return; 

    size_t size = t->AttributeSizes[AttributeIdx];

    if(size == 0 ) return;

    // a heap not initialized yet
    if (t->data[AttributeIdx] == NULL) {
        
        // allocate a heap
        t->data[AttributeIdx] = malloc(t->maxRecord * t->AttributeSizes[AttributeIdx]);
        if (!t->data[AttributeIdx]) return; // Allocation failed
        
    }

    // Cast to char* so pointer arithmetic works in bytes
    char* column_base = (char*)t->data[AttributeIdx];
    char* destination = column_base + (RecordIdx * size);

    memcpy(destination, data, size);

}

void* cDB_Table_getRecord(cDB_Table* t, uint32_t RecordIdx, uint32_t AttributeIdx)
{
    if(!t) return NULL;
    if(AttributeIdx >= t->maxAttribute) return NULL;
    if(RecordIdx >= t->maxRecord) return NULL; 

    if (t->data[AttributeIdx] == NULL) return NULL;

    char* column_base = (char*)t->data[AttributeIdx];
    size_t attr_size = t->AttributeSizes[AttributeIdx];

    return (void*)(column_base + (RecordIdx * attr_size));

}

bool cDB_Table_fwrite(cDB_Table* t, FILE* f)
{
    if(!t || !f) return false;

    fwrite(&t->maxAttribute, sizeof(uint32_t), 1, f);
    fwrite(&t->maxRecord, sizeof(uint32_t), 1, f);

    fwrite(t->AttributeSizes, sizeof(size_t), t->maxAttribute, f);

    fwrite(t->AttributeAlias, sizeof(char[65]), t->maxAttribute, f);
    fwrite(t->RecordAlias, sizeof(char[65]), t->maxRecord, f);

    for(uint32_t i = 0; i < t->maxAttribute; i++)
    {
        size_t col_size = t->maxRecord * t->AttributeSizes[i];

        if(t->data[i] != NULL)
        {
            fwrite(t->data[i], col_size, 1, f);
        }
        else 
        {
            void* zero_buf = calloc(1, col_size);
            if (zero_buf) {
                fwrite(zero_buf, 1, col_size, f);
                free(zero_buf);
            } else {
                return false; // Out of memory during write
            }
        }
    };

    return true;

};

bool cDB_Table_fread(cDB_Table* t, FILE* f)
{
    if(!t || !f) return false;

    // 1. Read core metadata dimensions 
    // (Note: ensure 't' was already initialized with these max bounds, 
    // or change this function to return a newly allocated table pointer!)
    fread(&t->maxAttribute, sizeof(uint32_t), 1, f);
    fread(&t->maxRecord, sizeof(uint32_t), 1, f);

    // 2. Read structural metadata and aliases
    fread(t->AttributeSizes, sizeof(size_t), t->maxAttribute, f);
    fread(t->AttributeAlias, sizeof(char[65]), t->maxAttribute, f);
    fread(t->RecordAlias, sizeof(char[65]), t->maxRecord, f);

    // 3. Read column data blocks
    for(uint32_t i = 0; i < t->maxAttribute; i++)
    {
        size_t col_size = t->maxRecord * t->AttributeSizes[i];

        if(col_size > 0)
        {
            // Allocate memory for this column's data block
            t->data[i] = malloc(col_size);
            if (!t->data[i]) return false; // Out of memory

            // READ from file into memory (Fixed from fwrite!)
            fread(t->data[i], 1, col_size, f);
        }
        else
        {
            t->data[i] = NULL;
        }
    }

    return true;
}