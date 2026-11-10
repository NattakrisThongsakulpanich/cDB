# cDB Library Documentation

`cDB` is a lightweight, columnar (attribute-major) database library written in C. It provides structured schema definitions, typed attributes, record persistence with binary serialization (`.cdb`), conditional queries, and compound logical filtering (`AND` / `OR`).

This project is developed as part of KMUTT Department of Computer Engineering.

---

## Table of Contents
1. [Architecture & Storage Model](#architecture--storage-model)
2. [Project Structure](#project-structure)
3. [Building the Library](#building-the-library)
4. [Quickstart Guide](#quickstart-guide)
5. [API Reference](#api-reference)
   - [Types & Enums](#types--enums)
   - [Database Lifecycle](#database-lifecycle)
   - [Table DDL](#table-ddl)
   - [Record DML](#record-dml)
   - [Querying & Result Sets](#querying--result-sets)
   - [Compound Conditions](#compound-conditions)
6. [Comprehensive Examples](#comprehensive-examples)
   - [1. Creating Database and Table](#1-creating-database-and-table)
   - [2. Inserting Records](#2-inserting-records)
   - [3. Simple Query with Condition](#3-simple-query-with-condition)
   - [4. Advanced Compound Query (AND / OR)](#4-advanced-compound-query-and--or)
   - [5. Updating Records with Compound Filter](#5-updating-records-with-compound-filter)
   - [6. Deleting Records](#6-deleting-records)
   - [7. Extracting Typed Data from ResultSet](#7-extracting-typed-data-from-resultset)
   - [8. Persistence & Reloading](#8-persistence--reloading)

---

## Architecture & Storage Model

Unlike traditional row-oriented relational databases, `cDB` organizes data by **Attribute** (Column) contiguously in memory:
$$\text{Memory Layout: } \texttt{data[AttributeIdx][RecordIdx]}$$

### Terminology:
- **Attribute**: A column or field with a specific name, fixed byte size, and data type.
- **Record**: A row containing values across all attributes for a single entry.
- **Alias**: A human-readable 64-character identifier assigned to each record and attribute.

### Binary File Persistence:
Calling `cDB_close()` automatically serializes the database to a binary file named `<db_name>.cdb` starting with a magic byte signature (`de1whoasked`), preserving schemas, metadata, aliases, and record data. Re-opening with `cDB_open()` restores state seamlessly.

---

## Project Structure

```text
cDB/
├── CMakeLists.txt     # CMake build configuration
├── README.md          # Project documentation
├── build/             # Build directory (generated)
└── src/
    ├── cDB.h          # Public cDB high-level API & definitions
    ├── cDB.c          # High-level database engine & queries
    ├── cDB_Table.h    # Low-level columnar table API
    ├── cDB_Table.c    # Columnar table memory & binary serialization
    ├── safefOC.h      # Safe file open/close utility declarations
    └── safefOC.c      # Safe file open/close implementation
```

---

## Building the Library

### Prerequisites
- C Compiler (`clang`, `gcc`, or MSVC)
- CMake (version 3.10+)

### Compile as Static Library (`.lib` / `.a`)
```bash
mkdir build
cd build
cmake ..
cmake --build .
```
This produces `libcDB_Library.a` (Linux/macOS) or `cDB_Library.lib` (Windows).

---

## Quickstart Guide

```c
#include "cDB.h"
#include <stdio.h>

int main(void) {
    // 1. Open or create database
    cDB_Database* db = cDB_open("SchoolDB", 10);

    // 2. Define schema
    cDB_AttributeDef attrs[] = {
        {"id",    CDB_TYPE_INT32,  0},
        {"name",  CDB_TYPE_STRING, 64},
        {"score", CDB_TYPE_DOUBLE, 0}
    };
    cDB_create_table(db, "students", attrs, 3, 100);

    // 3. Insert record
    int32_t id = 101;
    char name[64] = "Alice";
    double score = 88.5;
    const void* row[] = { &id, name, &score };
    cDB_insert(db, "students", "student_101", row);

    // 4. Select and print
    cDB_ResultSet* res = cDB_select(db, "students", NULL);
    cDB_print_result(res);
    cDB_free_result(res);

    // 5. Close and save to SchoolDB.cdb
    cDB_close(db);
    return 0;
}
```

---

## API Reference

### Types & Enums

#### `cDB_DataType`
Supported primitive and structured attribute types:
| Type Constant | Description | Size |
|---|---|---|
| `CDB_TYPE_INT32` | 32-bit signed integer | 4 bytes |
| `CDB_TYPE_INT64` | 64-bit signed integer | 8 bytes |
| `CDB_TYPE_FLOAT` | Single-precision floating point | 4 bytes |
| `CDB_TYPE_DOUBLE` | Double-precision floating point | 8 bytes |
| `CDB_TYPE_STRING` | Fixed-length string buffer | Custom (default: 64) |
| `CDB_TYPE_BYTES` | Raw binary / fixed struct | Custom (default: 8) |

#### `cDB_Op`
Comparison operators for condition matching:
- `CDB_OP_EQ` : Equal (`==`)
- `CDB_OP_NEQ`: Not Equal (`!=`)
- `CDB_OP_LT` : Less Than (`<`)
- `CDB_OP_LTE`: Less Than or Equal (`<=`)
- `CDB_OP_GT` : Greater Than (`>`)
- `CDB_OP_GTE`: Greater Than or Equal (`>=`)

#### `cDB_LogicConnective`
Logical connectives used to chain compound conditions:
- `CDB_LOGIC_NONE`: Terminal clause (end of condition chain)
- `CDB_LOGIC_AND` : Logical AND (`&&`)
- `CDB_LOGIC_OR`  : Logical OR (`||`)

---

### Database Lifecycle

#### `cDB_Database* cDB_open(const char* name, uint32_t max_tables)`
Opens a database. If `<name>.cdb` exists, it verifies the magic signature and loads the existing schema and data. If the file does not exist, a fresh database is initialized.
- **Parameters**:
  - `name`: Database name (and prefix for file `.cdb`).
  - `max_tables`: Maximum number of tables allowed.
- **Returns**: Pointer to `cDB_Database` or `NULL` on failure.

#### `void cDB_close(cDB_Database* db)`
Serializes all metadata and tables into `<name>.cdb`, frees all allocated heap memory, and closes the database.

---

### Table DDL

#### `bool cDB_create_table(cDB_Database* db, const char* table_name, const cDB_AttributeDef* attrs, uint32_t num_attrs, uint32_t max_records)`
Creates a new table with the given attribute definitions and capacity.
- **Parameters**:
  - `db`: Database pointer.
  - `table_name`: Table name (max 63 chars).
  - `attrs`: Array of `cDB_AttributeDef`.
  - `num_attrs`: Number of attributes in the array.
  - `max_records`: Maximum record capacity for this table.
- **Returns**: `true` on success, `false` if table already exists or capacity is exceeded.

#### `bool cDB_drop_table(cDB_Database* db, const char* table_name)`
Removes a table from the database and deallocates its memory.

---

### Record DML

#### `int64_t cDB_insert(cDB_Database* db, const char* table_name, const char* record_alias, const void** values)`
Inserts a new record into the specified table.
- **Parameters**:
  - `table_name`: Name of target table.
  - `record_alias`: Unique name/key for this record (or `NULL` to autogenerate `"record_<idx>"`).
  - `values`: Array of pointers to values, matching the attribute order in the schema.
- **Returns**: Record index (non-negative) on success, `-1` on error or when full.

#### `uint32_t cDB_update(cDB_Database* db, const char* table_name, const char* target_attr, const void* new_val, const cDB_Condition* cond)`
Updates an attribute across all records matching a single condition.
- **Returns**: Number of updated records.

#### `uint32_t cDB_delete(cDB_Database* db, const char* table_name, const cDB_Condition* cond)`
Deletes records matching the condition by clearing their alias and zeroing memory.
- **Returns**: Number of deleted records.

---

### Querying & Result Sets

#### `cDB_ResultSet* cDB_select(cDB_Database* db, const char* table_name, const cDB_Condition* cond)`
Executes a query with a single condition (pass `NULL` to select all records).
- **Returns**: Pointer to `cDB_ResultSet`. Must be freed with `cDB_free_result()`.

#### `void cDB_free_result(cDB_ResultSet* res)`
Deallocates memory associated with a query result set.

#### `void cDB_print_result(const cDB_ResultSet* res)`
Prints the query results formatted as an ASCII table to `stdout`.

#### Result Value Getters
Safely extracts typed attribute values from a specific row in the result set:
- `int32_t cDB_result_get_int32(res, row, attr_name, default_val)`
- `int64_t cDB_result_get_int64(res, row, attr_name, default_val)`
- `double  cDB_result_get_double(res, row, attr_name, default_val)`
- `const char* cDB_result_get_string(res, row, attr_name)`
- `const void* cDB_result_get(res, row, attr_name)`

---

### Compound Conditions

Allows chaining multiple conditions with `AND` and `OR` logic.

#### Functions:
- `cDB_CompoundCond* cDB_compound_init(void)`: Creates a new compound condition chain.
- `void cDB_compound_free(cDB_CompoundCond* comp)`: Frees the compound condition.
- `void cDB_compound_add(cDB_CompoundCond* comp, cDB_Condition cond, cDB_LogicConnective next_op)`: Appends a condition clause.
- `cDB_ResultSet* cDB_select_compound(db, table_name, comp)`: Selects records matching the compound condition.
- `uint32_t cDB_update_compound(db, table_name, target_attr, new_val, comp)`: Updates records matching the compound condition.
- `uint32_t cDB_delete_compound(db, table_name, comp)`: Deletes records matching the compound condition.

---

## Comprehensive Examples

### 1. Creating Database and Table

```c
#include "cDB.h"

int main(void) {
    // Initialize or load database (supports up to 5 tables)
    cDB_Database* db = cDB_open("CompanyDB", 5);

    // Define table attributes
    cDB_AttributeDef employee_attrs[] = {
        {"id",         CDB_TYPE_INT32,  0},
        {"name",       CDB_TYPE_STRING, 48},
        {"department", CDB_TYPE_STRING, 32},
        {"salary",     CDB_TYPE_DOUBLE, 0},
        {"active",     CDB_TYPE_INT32,  0}
    };

    // CREATE TABLE employees (...) with capacity for 500 records
    cDB_create_table(db, "employees", employee_attrs, 5, 500);

    cDB_close(db);
    return 0;
}
```

---

### 2. Inserting Records

```c
#include "cDB.h"

void add_employees(cDB_Database* db) {
    // Record 1
    int32_t id1 = 1;
    char name1[48] = "Sarah Connor";
    char dept1[32] = "Security";
    double salary1 = 85000.0;
    int32_t active1 = 1;
    const void* row1[] = { &id1, name1, dept1, &salary1, &active1 };
    cDB_insert(db, "employees", "emp_1", row1);

    // Record 2
    int32_t id2 = 2;
    char name2[48] = "John Doe";
    char dept2[32] = "Engineering";
    double salary2 = 92000.0;
    int32_t active2 = 1;
    const void* row2[] = { &id2, name2, dept2, &salary2, &active2 };
    cDB_insert(db, "employees", "emp_2", row2);

    // Record 3
    int32_t id3 = 3;
    char name3[48] = "Miles Dyson";
    char dept3[32] = "Engineering";
    double salary3 = 115000.0;
    int32_t active3 = 0;
    const void* row3[] = { &id3, name3, dept3, &salary3, &active3 };
    cDB_insert(db, "employees", "emp_3", row3);
}
```

---

### 3. Simple Query with Condition

```c
// SELECT * FROM employees WHERE department == "Engineering"
char target_dept[32] = "Engineering";
cDB_Condition cond = {
    .attr_name = "department",
    .op = CDB_OP_EQ,
    .val = target_dept
};

cDB_ResultSet* res = cDB_select(db, "employees", &cond);
cDB_print_result(res);
cDB_free_result(res);
```

---

### 4. Advanced Compound Query (AND / OR)

You can chain multiple criteria together using `cDB_CompoundCond`:

$$\text{Query: } \texttt{department == "Engineering" AND salary >= 90000 AND active == 1}$$

```c
#include "cDB.h"

void query_active_senior_engineers(cDB_Database* db) {
    cDB_CompoundCond* comp = cDB_compound_init();

    char dept[32] = "Engineering";
    double min_salary = 90000.0;
    int32_t is_active = 1;

    // Clause 1: department == "Engineering" AND ...
    cDB_Condition c1 = {
        .attr_name = "department",
        .op = CDB_OP_EQ,
        .val = dept
    };
    cDB_compound_add(comp, c1, CDB_LOGIC_AND);

    // Clause 2: salary >= 90000.0 AND ...
    cDB_Condition c2 = {
        .attr_name = "salary",
        .op = CDB_OP_GTE,
        .val = &min_salary
    };
    cDB_compound_add(comp, c2, CDB_LOGIC_AND);

    // Clause 3: active == 1 (Terminal: CDB_LOGIC_NONE)
    cDB_Condition c3 = {
        .attr_name = "active",
        .op = CDB_OP_EQ,
        .val = &is_active
    };
    cDB_compound_add(comp, c3, CDB_LOGIC_NONE);

    // Execute query
    cDB_ResultSet* res = cDB_select_compound(db, "employees", comp);
    
    printf("--- Active Senior Engineers ---\n");
    cDB_print_result(res);

    // Cleanup
    cDB_free_result(res);
    cDB_compound_free(comp);
}
```

---

### 5. Updating Records with Compound Filter

$$\text{Update: } \texttt{UPDATE employees SET salary = 98000.0 WHERE department == "Engineering" AND active == 1}$$

```c
cDB_CompoundCond* comp = cDB_compound_init();

char dept[32] = "Engineering";
int32_t is_active = 1;
double updated_salary = 98000.0;

cDB_Condition c1 = { .attr_name = "department", .op = CDB_OP_EQ, .val = dept };
cDB_Condition c2 = { .attr_name = "active",     .op = CDB_OP_EQ, .val = &is_active };

cDB_compound_add(comp, c1, CDB_LOGIC_AND);
cDB_compound_add(comp, c2, CDB_LOGIC_NONE);

uint32_t count = cDB_update_compound(db, "employees", "salary", &updated_salary, comp);
printf("Updated salaries for %u employee(s).\n", count);

cDB_compound_free(comp);
```

---

### 6. Deleting Records

```c
// DELETE FROM employees WHERE active == 0
int32_t inactive_val = 0;
cDB_Condition cond_inactive = {
    .attr_name = "active",
    .op = CDB_OP_EQ,
    .val = &inactive_val
};

uint32_t deleted = cDB_delete(db, "employees", &cond_inactive);
printf("Purged %u inactive employee record(s).\n", deleted);
```

---

### 7. Extracting Typed Data from ResultSet

Instead of just printing with `cDB_print_result`, you can iterate through the result set programmatically:

```c
cDB_ResultSet* res = cDB_select(db, "employees", NULL);

for (uint32_t i = 0; i < res->count; i++) {
    int32_t id         = cDB_result_get_int32(res, i, "id", -1);
    const char* name   = cDB_result_get_string(res, i, "name");
    const char* dept   = cDB_result_get_string(res, i, "department");
    double salary      = cDB_result_get_double(res, i, "salary", 0.0);

    printf("[%d] %s | Dept: %s | Salary: $%.2f\n", id, name, dept, salary);
}

cDB_free_result(res);
```

---

### 8. Persistence & Reloading

Data written to `cDB` persists automatically to disk when closed:

```c
// Session 1: Save
cDB_Database* db1 = cDB_open("InventoryDB", 5);
// ... create tables and insert records ...
cDB_close(db1); // Automatically writes to InventoryDB.cdb

// Session 2: Reload on next program start
cDB_Database* db2 = cDB_open("InventoryDB", 5); // Restores all tables & records!
cDB_ResultSet* res = cDB_select(db2, "products", NULL);
cDB_print_result(res);
cDB_free_result(res);
cDB_close(db2);
```
