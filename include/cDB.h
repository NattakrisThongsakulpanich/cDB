#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <xkeycheck.h>

#include "./cDB_Table.h"

extern const char MAGIC_BYTE[12]; /* use to verify a .cdb file*/
#define STD_CDB_STRLEN 65 /* 64+1 reserve for '\0' */
#define CDB_OUT_OF_RANGE uint64_t
#define cDB_Record uint64_t

typedef enum
{
	CDB_BOOL,
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

// File/IO
bool cDB_Save(cDB_Database* db, const char filename[65]);
bool cDB_Open(cDB_Database* db, const char filename[65]);
bool cDB_Proj2CSV(cDB_Database* db, const cDB_Projection* proj, const char filename[65]);

// Table manipulation
bool cDB_newTable(cDB_Database* db, const char name[STD_CDB_STRLEN], const uint32_t maxRecord, const uint32_t maxAttribute);

// Attribute operations
cDB_Attribute cDB_createAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN], cDB_Datatype type, size_t custom_size);
cDB_Attribute cDB_getAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN]);
void cDB_setAttributeAlias(cDB_Database *db, cDB_Attribute attrib, const char alias[STD_CDB_STRLEN]);

// Record operations
cDB_Record cDB_getRecord(cDB_Database* db, const char alias[STD_CDB_STRLEN]);
void cDB_setRecordAlias(cDB_Database *db, cDB_Record record, const char alias[STD_CDB_STRLEN]);

//-----------------------------------Value access----------------------------------------
// set
void cDB_set_t(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, void* data);
// Typed set
void cDB_set_BOOL(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const bool data);
void cDB_set_I32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const int32_t data);
void cDB_set_I64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const int64_t data);
void cDB_set_UI32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const uint32_t data);
void cDB_set_UI64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const uint64_t data);
void cDB_set_F32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const float data);
void cDB_set_F64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const double data);
void cDB_set_STR(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, const char* data); // This copy string until '\0' is found
// get
void* cDB_get_t(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
// Typed set
bool 		cDB_get_BOOL(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
int32_t 	cDB_get_I32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
int64_t 	cDB_get_I64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
uint32_t 	cDB_get_UI32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
uint64_t 	cDB_get_UI64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
float 		cDB_get_F32(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
double 		cDB_get_F64(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);
char* 		cDB_get_STR(cDB_Database* db, cDB_Record record, cDB_Attribute attrib);

//-----------------------------------select----------------------------------------

// Selection & Results
void 		cDB_clearResult(cDB_Database* db);
bool 		cDB_select(cDB_Database* db, cDB_Predicate predicate, void* user_data);
bool 		cDB_cpyResult(const cDB_Database* db, cDB_Result* dst); //cDB_resultGetRecord -> cDB_cpyResult()
const cDB_Result* cDB_getResult(const cDB_Database* db);

//----------------------------------projections------------------------------------

// Projections & Printing
cDB_Projection* cDB_getProjection(cDB_Database* db, cDB_Record* records, uint64_t nRecords, const uint64_t* attrib_indices, uint64_t nAttributes);
cDB_Projection* cDB_getRProjection(cDB_Database* db); // get Result Projection
cDB_Projection* cDB_getTProjection(cDB_Database* db); // get Table Projection
bool cDB_Proj2CSV(cDB_Database *db, const cDB_Projection *proj, const char *filename);
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
