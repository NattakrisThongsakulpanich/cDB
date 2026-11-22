#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

//cDB is Column(Record) major database
typedef struct
{

	const char name[65];

	void** data; // [AttributeIdx][RecordIdx]

	size_t* attributeSizes;
	char (*attributeNames)[65];
	uint64_t attributeCapacity;
	
	uint64_t recordCapacity;
	char (*recordNames)[65];

} cDB_Table;

// Constructor / Destructor
cDB_Table* cdb_table_Create(const uint64_t attributeCapacity, const uint64_t recordCapacity, const char* name);
void cdb_table_Destroy(cDB_Table* t);

// Gets
uint64_t cdb_table_getAttributeIndex(cDB_Table* t, const char* alias);
uint64_t cdb_table_getRecordIndex(cDB_Table* t, const char* alias);
void* cdb_table_get(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx);

// Sets
void cdb_table_setRecordName(cDB_Table* t, uint64_t RecordIdx, const char* alias);
void cdb_table_setAttributeName(cDB_Table* t, uint64_t AttributeIdx, const char* alias);
void cdb_table_copyRecordName(cDB_Table* t, uint64_t RecordIdx, char* buffer);
void cdb_table_copyAttributeName(cDB_Table* t, uint64_t AttributeIdx, char* buffer);

void cdb_table_setAttributeSize(cDB_Table* t, uint64_t AttributeIdx, size_t size);
void cdb_table_set(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx, const void* data);

// IO
bool cdb_table_writeFile(cDB_Table* t, FILE* f);
__declspec(deprecated("Use cdb_table_readFile(FILE* f) instead")) bool cdb_table_read_file_into(cDB_Table* t, FILE* f);
cDB_Table* cdb_table_readFile(FILE* f);