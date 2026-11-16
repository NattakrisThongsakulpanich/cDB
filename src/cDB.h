#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "./cDB_Table.h"

extern const char MAGIC_BYTE[12];
#define STD_CDB_STRLEN 65 /* 64+1 reserve for '\0' */
#define CDB_OUT_OF_RANGE uint64_t
#define cDB_Record uint64_t

typedef enum
{
	CDB_INT32,
	CDB_INT64,
	CDB_UINT32,
	CDB_UINT64,
	CDB_FLOAT32,
	CDB_FLOAT64,
	CDB_STRING,
	CDB_CUSTOM,
} cDB_Datatype;

typedef enum
{
	CDB_SORTDIR_ASCEND,
	CDB_SORTDIR_DESCEND,
} cDB_SortDirection;

typedef struct
{
	char UpperLeft_Corner;
	char UpperRight_Corner;
	char LowerLeft_Corner;
	char LowerRight_Corner;
	char VerticalLine;
	char HorizontalLine;
} cDB_PrintStyle;

typedef struct
{
	char name[STD_CDB_STRLEN];
	uint64_t idx;
	size_t size;
	cDB_Datatype type;
	bool isValid;
} cDB_Attribute;

typedef struct
{
	cDB_Record* projRecords; // an Array of Record associated with a query
	uint64_t* projAttributes; // pointer to the projected attributes associated with a query
	uint64_t nRecords; // number of records in this projection
	uint64_t nAttributes; // number of Attributes in this projection
} cDB_Projection;

typedef struct
{
	cDB_Record* inCondRecords; // Records with condition met
	uint64_t nResult; // number of Results
} cDB_Result;

typedef struct
{
	cDB_Table* table; 
	cDB_Result qResult; // result from select
	bool isSaved;
} cDB_Database;

typedef bool (*cDB_Predicate)(cDB_Database* db, cDB_Record record, void* user_data);

// Database lifecycle
cDB_Database* cDB_Init(void);
void cDB_Close(cDB_Database* db);

// Persistence
bool cDB_Save(cDB_Database* db, const char filename[65]);
bool cDB_Open(cDB_Database* db, const char filename[65]);

// Table manipulation
bool cDB_newTable(cDB_Database* db, const char name[STD_CDB_STRLEN], const uint32_t maxRecord, const uint32_t maxAttribute);

// Attribute operations
cDB_Attribute cDB_createAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN], cDB_Datatype type, size_t custom_size);
cDB_Attribute cDB_getAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN]);
void cDB_setAttributeAlias(cDB_Database *db, cDB_Attribute attrib, const char alias[STD_CDB_STRLEN]);

// Record operations
cDB_Record cDB_getRecord(cDB_Database* db, const char alias[STD_CDB_STRLEN]);
void cDB_setRecordAlias(cDB_Database *db, cDB_Record record, const char alias[STD_CDB_STRLEN]);
void cDB_setRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, void* data);
void* cDB_getRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);

// Selection & Results
void cDB_clearResult(cDB_Database* db);
bool cDB_select(cDB_Database* db, cDB_Predicate predicate, void* user_data);
cDB_Record cDB_resultGetRecord(const cDB_Database* db, uint64_t result_idx);
uint64_t cDB_resultLen(const cDB_Database* db);
void* cDB_resultGetValue(cDB_Database* db, uint64_t result_idx, cDB_Attribute attrib);

// Projections & Printing
cDB_Projection* cDB_getProjection(cDB_Database* db, cDB_Record* records, uint64_t nRecords, const uint64_t* attrib_indices, uint64_t nAttributes);
cDB_Projection* cDB_getResultProjection(cDB_Database* db);
cDB_Projection* cDB_getTableProjection(cDB_Database* db);
void cDB_freeProjection(cDB_Projection* proj);
void cDB_printProjection(
	const cDB_Database* db, 
	const cDB_Projection* proj, 
	bool hSeperator, 
	bool vSeperator, 
	cDB_PrintStyle* printStyle, 
	uint8_t* hPadding, 
	uint8_t* vPadding
);
