#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

//cDB is Column(Record) major database
typedef struct
{

	const char name[65];

	void** data; // [AttributeIdx][RecordIdx]

	size_t* AttributeSizes;
	char (*AttributeAlias)[65];
	uint64_t maxAttribute;
	
	uint64_t maxRecord;
	char (*RecordAlias)[65];

} cDB_Table;

// Constructor / Destructor
cDB_Table* cDB_Table_new(const uint64_t maxAttribute, const uint64_t maxRecord, const char* name);
void cDB_Table_destroy(cDB_Table* t);

// Gets
uint64_t cDB_Table_getAttributeIdx(cDB_Table* t, const char* alias);
uint64_t cDB_Table_getRecordIdx(cDB_Table* t, const char* alias);
void* cDB_Table_getRecord(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx);
void* cDB_Table_getAttribProjection(cDB_Table* t, uint64_t AttributeIdx);

// Sets
void cDB_Table_setRecordAlias(cDB_Table* t, uint64_t RecordIdx, const char* alias);
void cDB_Table_setAttributeAlias(cDB_Table* t, uint64_t AttributeIdx, const char* alias);
void cDB_Table_setAttributeSize(cDB_Table* t, uint64_t AttributeIdx, size_t size);
void cDB_Table_setRecord(cDB_Table* t, uint64_t RecordIdx, uint64_t AttributeIdx, void* data);

// IO
bool cDB_Table_fwrite(cDB_Table* t, FILE* f);
bool cDB_Table_fread(cDB_Table* t, FILE* f);
