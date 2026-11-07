#pragma once

#include "cDB_Table.h"
#include <stdint.h>
#include <stdbool.h>

// Supported column data types
typedef enum {
    CDB_TYPE_INT32,
    CDB_TYPE_INT64,
    CDB_TYPE_FLOAT,
    CDB_TYPE_DOUBLE,
    CDB_TYPE_STRING, // Fixed-size string buffer up to size bytes
    CDB_TYPE_BYTES   // Raw binary / struct
} cDB_DataType;

// Column schema definition for creating tables
typedef struct {
    const char* name;
    cDB_DataType type;
    size_t size; // For strings / bytes; if 0 for primitives, auto-determined
} cDB_ColumnDef;

// High-level Database structure holding tables
typedef struct {
    char name[64]; // name of a Database
    uint32_t max_tables; // Max number of tables
    uint32_t table_count;
    char (*table_names)[64]; 
    
    // This is not Matrix of cDB_Table it's a pointer to a heap
    // cDB_Table_init returns a heap to a Table
    cDB_Table** tables; // Tables
    
    cDB_DataType** col_types; // [table_idx][col_idx]
} cDB_Database;

// Comparison operators for WHERE filter conditions
typedef enum {
    CDB_OP_EQ,  // ==
    CDB_OP_NEQ, // !=
    CDB_OP_LT,  // <
    CDB_OP_LTE, // <=
    CDB_OP_GT,  // >
    CDB_OP_GTE  // >=
} cDB_Op;

// Where filter condition
typedef struct {
    const char* col_name;
    cDB_Op op;
    const void* val; // Pointer to comparison value
} cDB_Condition;

// Query result set structure (like SQL cursor/result)
typedef struct {
    cDB_Database* db;
    uint32_t table_idx;
    uint32_t* row_indices;
    uint32_t count;
    uint32_t capacity;
} cDB_ResultSet;

// --- Database Lifecycle ---
cDB_Database* cDB_open(const char* name, uint32_t max_tables);
void cDB_close(cDB_Database* db);

// --- DDL (CREATE TABLE, DROP TABLE) ---
bool cDB_create_table(cDB_Database* db, const char* table_name, const cDB_ColumnDef* cols, uint32_t num_cols, uint32_t max_records);
bool cDB_drop_table(cDB_Database* db, const char* table_name);

// --- DML (INSERT, UPDATE, DELETE) ---
// Insert a record by providing an array of pointers to column values in schema order
int64_t cDB_insert(cDB_Database* db, const char* table_name, const char* record_alias, const void** values);

// Update columns for rows matching condition(s)
uint32_t cDB_update(cDB_Database* db, const char* table_name, const char* target_col, const void* new_val, const cDB_Condition* cond);

// Delete rows matching condition(s) (marks alias or zeroes data)
uint32_t cDB_delete(cDB_Database* db, const char* table_name, const cDB_Condition* cond);

// --- DQL (SELECT) ---
// Select rows matching an optional condition (cond can be NULL to select all)
cDB_ResultSet* cDB_select(cDB_Database* db, const char* table_name, const cDB_Condition* cond);
void cDB_free_result(cDB_ResultSet* res);

// Value getters from ResultSet
const void* cDB_result_get(const cDB_ResultSet* res, uint32_t result_row_idx, const char* col_name);
int32_t cDB_result_get_int32(const cDB_ResultSet* res, uint32_t result_row_idx, const char* col_name, int32_t default_val);
int64_t cDB_result_get_int64(const cDB_ResultSet* res, uint32_t result_row_idx, const char* col_name, int64_t default_val);
double cDB_result_get_double(const cDB_ResultSet* res, uint32_t result_row_idx, const char* col_name, double default_val);
const char* cDB_result_get_string(const cDB_ResultSet* res, uint32_t result_row_idx, const char* col_name);

// Pretty print a result set to stdout (table format)
void cDB_print_result(const cDB_ResultSet* res);
