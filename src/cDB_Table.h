#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

//cDB is Column(Record) major database
typedef struct
{
	void** data; // [AttributeIdx][RecordIdx]

	size_t* AttributeSizes;
	char (*AttributeAlias)[65];
	uint32_t maxAttribute;
	
	uint32_t maxRecord;
	char (*RecordAlias)[65];

} cDB_Table;

typedef struct
{
	uint32_t idx;
	char alias[65];
} cDB_Table_AliasTuple;

typedef struct
{
	size_t* AttributeSizes;
	uint32_t maxRecord;
	uint32_t maxAttribute;
} cDB_Table_fMeta;

cDB_Table* cDB_Table_init(const uint32_t maxAttribute, const uint32_t maxRecord);

void cDB_Table_destroy(cDB_Table* t);

uint32_t cDB_Table_getAttributeIdx(cDB_Table* t, const char* alias);

uint32_t cDB_Table_getRecordIdx(cDB_Table* t, const char* alias);

void cDB_Table_setRecordAlias(cDB_Table* t, uint32_t RecordIdx, const char* alias);

void cDB_Table_setAttributeAlias(cDB_Table* t, uint32_t AttributeIdx, const char* alias);

void cDB_Table_setAttributeSize(cDB_Table* t, uint32_t AttributeIdx, size_t size);

void cDB_Table_setRecord(cDB_Table* t, uint32_t RecordIdx, uint32_t AttributeIdx, void* data);

void* cDB_Table_getRecord(cDB_Table* t, uint32_t RecordIdx, uint32_t AttributeIdx);

bool cDB_Table_fwrite(cDB_Table* t, FILE* f);

bool cDB_Table_fread(cDB_Table* t, FILE* f);