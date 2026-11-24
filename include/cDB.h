#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "./cDB_Table.h"

extern const char MAGIC_BYTE[12]; /* use to verify a .cdb file*/
#define CDB_STD_STRLEN 65 /* 64+1 reserve for '\0' */
#define CDB_OUT_OF_RANGE uint64_t
#define cDB_Record uint64_t
#define CDB_PRINT_MAX_COL_WIDTH 40

typedef enum
{
	CDB_CUSTOM,
	CDB_BOOL,
	CDB_INT32,
	CDB_INT64,
	CDB_UINT32,
	CDB_UINT64,
	CDB_FLOAT32,
	CDB_FLOAT64,
	CDB_STRING,
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
	char name[CDB_STD_STRLEN];
	uint64_t index;
	size_t size;
	cDB_Datatype type;
	bool isValid;
} cDB_Attribute;

typedef struct
{
	cDB_Record* projectedRecords; // an Array of Record associated with a query
	uint64_t* projectedAttributes; // pointer to the projected attributes associated with a query
	uint64_t recordCount; // number of records in this projection
	uint64_t attributeCount; // number of Attributes in this projection
} cDB_Projection;

typedef struct
{
	cDB_Record* matchingRecords; // Records with condition met
	uint64_t resultCount; // number of Results
} cDB_Result;

typedef struct
{
	cDB_Table* table; 
	cDB_Result result; // result from select
	cDB_Datatype* type_ref;
	bool isSaved;
} cDB_Database;

typedef bool (*cDB_Predicate)(cDB_Database* db, cDB_Record record, void* user_data);

// Database lifecycle
cDB_Database* cdb_Create(void);
void cdb_Destroy(cDB_Database* db);

// File/IO
bool cdb_save(cDB_Database* db, const char filename[65]);
bool cdb_open(cDB_Database* db, const char filename[65]);

bool cdb_importCSV(cDB_Database*db, const char filename[65], cDB_Datatype* format);
bool cdb_exportCSV(cDB_Database* db, const cDB_Projection* proj, const char filename[65]);

// Table manipulation
bool cdb_CreateTable(cDB_Database* db, const char name[CDB_STD_STRLEN], const uint32_t recordCapacity, const uint32_t attributeCapacity);

// Attribute operations
cDB_Attribute cdb_CreateAttribute(cDB_Database* db, const char alias[CDB_STD_STRLEN], cDB_Datatype type, size_t custom_size);
cDB_Attribute cdb_getAttribute(cDB_Database* db, const char alias[CDB_STD_STRLEN]);
cDB_Attribute cdb_getAttribute_byIndex(cDB_Database* db, uint64_t attrib_idx);
void cdb_setAttributeName(cDB_Database *db, cDB_Attribute* attrib, const char alias[CDB_STD_STRLEN]);

// Record operations
cDB_Record cdb_get_record(cDB_Database* db, const char alias[CDB_STD_STRLEN]);
void cdb_set_recordName(cDB_Database *db, cDB_Record record, const char alias[CDB_STD_STRLEN]);
void cdb_swapRecord(cDB_Database* db, uint64_t record1, uint64_t record2); //swap everything include attributes and RecordName

//-----------------------------------Value access----------------------------------------
// set
void cdb_set_t(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const void* data);
// Typed set
void cdb_set_bool(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const bool data);
void cdb_set_i32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const int32_t data);
void cdb_set_i64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const int64_t data);
void cdb_set_u32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const uint32_t data);
void cdb_set_u64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const uint64_t data);
void cdb_set_f32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const float data);
void cdb_set_f64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const double data);
void cdb_set_string(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const char* data); // This copy string until '\0' is found
// get
void* cdb_get_t(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
// Typed get
bool 		cdb_get_bool(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
int32_t 	cdb_get_i32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
int64_t 	cdb_get_i64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
uint32_t 	cdb_get_u32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
uint64_t 	cdb_get_u64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
float 		cdb_get_f32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
double 		cdb_get_f64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);
char* 		cdb_get_string(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib);

uint64_t cdb_format_value(cDB_Database* db, cDB_Record record, cDB_Attribute* aattrib, char* buffer); // this returns number of written character

//-----------------------------------select----------------------------------------

// Selection & Results
void 		cdb_clearResult(cDB_Database* db);
bool 		cdb_select(cDB_Database* db, cDB_Predicate predicate, void* user_data);
bool 		cdb_copyResult(const cDB_Database* db, cDB_Result* dst); //cDB_resultGetRecord -> cdb_copyResult()
const cDB_Result* cdb_getResult(const cDB_Database* db);

//-----------------------------------sort------------------------------------------
void cdb_sort_DB(cDB_Database *db, cDB_Attribute* by, cDB_SortDirection direction);
void cdb_sort_Projection(cDB_Database* db, cDB_Projection* projection, cDB_Attribute* by, cDB_SortDirection direction);

//----------------------------------projections------------------------------------

// Projections & Printing
cDB_Projection* cdb_CreateProjection(cDB_Database* db, cDB_Record* records, uint64_t recordCount, const uint64_t* attrib_indices, uint64_t attributeCount);
cDB_Projection* cdb_CreateResultProjection(cDB_Database* db); // get Result Projection
cDB_Projection* cdb_CreateTableProjection(cDB_Database* db); // get Table Projection
void cdb_freeProjection(cDB_Projection* proj);
void cdb_printProjection(
	cDB_Database* db, 
	cDB_Projection* proj, 
	bool hSeperator, 
	bool vSeperator, 
	cDB_PrintStyle* printStyle
);
