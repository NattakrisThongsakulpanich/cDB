#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/cDB_Table.h"

cDB_Table* cdb_table_Create(const uint64_t attributeCapacity, const uint64_t recordCapacity, const char* name)
{

    // Allocate the master table container struct on the heap
    cDB_Table* t = (cDB_Table*)malloc(sizeof(cDB_Table));
    if(!t) return NULL;

    t->attributeCapacity = attributeCapacity;
    t->recordCapacity = recordCapacity;

    if (name) {
        strncpy((char*)t->name, name, sizeof(t->name) - 1);
        ((char*)t->name)[sizeof(t->name) - 1] = '\0';
    } else {
        ((char*)t->name)[0] = '\0';
    }

    // Allocate trackers
    t->attributeSizes = (size_t*)calloc(attributeCapacity, sizeof(size_t));
    t->data = (void**)calloc(attributeCapacity, sizeof(void*));

    // Allocate alias grid buffers (attributeCapacity / recordCapacity rows of 65 chars)
    t->attributeNames = (char(*)[65])calloc(attributeCapacity, sizeof(char[65]));
    t->recordNames = (char(*)[65])calloc(recordCapacity, sizeof(char[65]));

    // Handling Allocations if fails (destroy the table) and return NULL
    if (!t->attributeSizes || !t->data || !t->attributeNames || !t->recordNames) {
        cdb_table_Destroy(t);
        return NULL;
    }

    return t;

};

void cdb_table_Destroy(cDB_Table* t)
{
    if (!t) return;

    // Free each individual column's contiguous memory block
    if (t->data) {
        for (uint32_t i = 0; i < t->attributeCapacity; i++) {
            // Free whole chunk of Records array if data[AttributeIdx] exist
            if (t->data[i]) {
                free(t->data[i]); 
            }
        }
        free(t->data); // Then free the data** itself
    }

    // Free metadata and alias arrays
    free(t->attributeSizes);
    free(t->attributeNames);
    free(t->recordNames);

    // Free the table struct itself
    free(t);
}


uint64_t cdb_table_getAttributeIndex(cDB_Table* t, const char* alias)
{
    if(!t || !alias) return UINT64_MAX;

    for(uint64_t i = 0; i < t->attributeCapacity; i++)
    {
        if(strcmp(t->attributeNames[i], alias) == 0)
        {
            return i;
        }
    }

    return UINT64_MAX;

};

uint64_t cdb_table_getRecordIndex(cDB_Table* t, const char* alias)
{
    if(!t || !alias) return UINT64_MAX;

    for(uint64_t i = 0; i < t->recordCapacity; i++)
    {
        if(strcmp(t->recordNames[i], alias) == 0)
        {
            return i;
        }
    }

    return UINT64_MAX;
};

void cdb_table_setRecordName(cDB_Table* t, uint64_t RecordIdx, const char* alias)
{
    if(!t || !alias) return;
    if(RecordIdx >= t->recordCapacity) return;

    strncpy(t->recordNames[RecordIdx], alias, 65);
    t->recordNames[RecordIdx][64] = '\0';

};

void cdb_table_setAttributeName(cDB_Table* t, uint64_t AttributeIdx, const char* alias)
{
    if(!t || !alias) return;
    if(AttributeIdx >= t->attributeCapacity) return;

    strncpy(t->attributeNames[AttributeIdx], alias, 65);
    t->attributeNames[AttributeIdx][64] = '\0';
};

void cdb_table_copyRecordName(cDB_Table* t, uint64_t RecordIdx, char* buffer)
{
    if(!t || RecordIdx >= t->recordCapacity) return;

    strncpy(buffer, t->recordNames[RecordIdx], 65);

};

void cdb_table_copyAttributeName(cDB_Table* t, uint64_t AttributeIdx, char* buffer)
{
    if(!t || AttributeIdx >= t->attributeCapacity) return;

    strncpy(buffer, t->attributeNames[AttributeIdx], 65);
};

void cdb_table_setAttributeSize(cDB_Table* t, uint64_t AttributeIdx, size_t size)
{
    if(!t) return;
    if(size == 0) return;
    if(AttributeIdx >= t->attributeCapacity) return;

    t->attributeSizes[AttributeIdx] = size;
};


void cdb_table_set(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx, const void* data)
{
    if(!t || !data) return;
    if(AttributeIdx >= t->attributeCapacity) return;
    if(RecordIdx >= t->recordCapacity) return; 

    size_t size = t->attributeSizes[AttributeIdx];

    if(size == 0 ) return;

    // a heap not initialized yet
    if (t->data[AttributeIdx] == NULL) {
        
        // allocate a heap
        t->data[AttributeIdx] = malloc(t->recordCapacity * t->attributeSizes[AttributeIdx]);
        if (!t->data[AttributeIdx]) return; // Allocation failed
        
    }

    // Cast to char* so pointer arithmetic works in bytes
    char* column_base = (char*)t->data[AttributeIdx];
    char* destination = column_base + (RecordIdx * size);

    memcpy(destination, data, size);

}

void* cdb_table_get(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx)
{
    if(!t) return NULL;
    if(AttributeIdx >= t->attributeCapacity) return NULL;
    if(RecordIdx >= t->recordCapacity) return NULL; 

    if (t->data[AttributeIdx] == NULL) return NULL;

    char* column_base = (char*)t->data[AttributeIdx];
    size_t attr_size = t->attributeSizes[AttributeIdx];

    return (void*)(column_base + (RecordIdx * attr_size));

}

bool cdb_table_writeFile(cDB_Table* t, FILE* f)
{
    if(!t || !f) return false;

	//writes metadata
    fwrite(&t->name, sizeof(char), 65, f);
    fwrite(&t->attributeCapacity, sizeof(uint64_t), 1, f);
    fwrite(&t->recordCapacity, sizeof(uint64_t), 1, f);

    fwrite(t->attributeSizes, sizeof(size_t), t->attributeCapacity, f);
    // this save all alias all at once
    fwrite(t->attributeNames, sizeof(char[65]), t->attributeCapacity, f);
    fwrite(t->recordNames, sizeof(char[65]), t->recordCapacity, f);

    for(uint64_t i = 0; i < t->attributeCapacity; i++)
    {
        size_t col_size = t->recordCapacity * t->attributeSizes[i];

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

//**DEPRECATED**
// * Have to guess or read the meta first before copy the data into a table so it's not safe.
bool cdb_table_read_file_into(cDB_Table* t, FILE* f)
{
    if(!t || !f) return false;

    // Read core metadata dimensions
    fread((void*)t->name, sizeof(char), 65, f);
    fread(&t->attributeCapacity, sizeof(uint64_t), 1, f);
    fread(&t->recordCapacity, sizeof(uint64_t), 1, f);

    // Read structural metadata and aliases
    fread(t->attributeSizes, sizeof(size_t), t->attributeCapacity, f);
    fread(t->attributeNames, sizeof(char[65]), t->attributeCapacity, f);
    fread(t->recordNames, sizeof(char[65]), t->recordCapacity, f);

    // Read column data blocks
    for(uint64_t i = 0; i < t->attributeCapacity; i++)
    {
        size_t col_size = t->recordCapacity * t->attributeSizes[i];

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

//new function to replace Table_fread()
cDB_Table* cdb_table_readFile(FILE* f)
{
    if(!f) return NULL;

    uint64_t attributeCapacity = 0;
    uint64_t recordCapacity = 0;
    char t_name[65];

    // Read core metadata dimensions 
    fread(&t_name, sizeof(char), 65, f);
    fread(&attributeCapacity, sizeof(uint64_t), 1, f);
    fread(&recordCapacity, sizeof(uint64_t), 1, f);

    if(recordCapacity == 0 || attributeCapacity == 0) return NULL;

    cDB_Table* t = cdb_table_Create(attributeCapacity, recordCapacity, t_name); //already set a max Record and Attribute and name in this section

    // Read structural metadata and aliases
    fread(t->attributeSizes, sizeof(size_t), t->attributeCapacity, f);
    fread(t->attributeNames, sizeof(char[65]), t->attributeCapacity, f);
    fread(t->recordNames, sizeof(char[65]), t->recordCapacity, f);

    // Read column data blocks
    for(uint64_t i = 0; i < t->attributeCapacity; i++)
    {
        size_t col_size = t->recordCapacity * t->attributeSizes[i];

        if(col_size > 0)
        {
            // Allocate memory for this column's data block
            t->data[i] = malloc(col_size);
            if (!t->data[i])
            { 
                cdb_table_Destroy(t);
                return NULL; // Out of memory
            }

            // READ from file into memory (Fixed from fwrite!)
            fread(t->data[i], 1, col_size, f);
        }
        else
        {
            t->data[i] = NULL;
        }
    }

    return t;
}
