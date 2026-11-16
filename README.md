# cDB Library Documentation

`cDB` is a lightweight, columnar (attribute-major) database library written in C. It provides structured schema definitions, strongly-typed attributes, record management, explicit binary serialization (`.cdb`), flexible functional querying via custom predicates, and simple projection/custom formatting tools.

This project is developed as part of the KMUTT Department of Computer Engineering, Computer Programming 101.

NOTICE: Due to a tight project deadline. This project is AI assisted approximately to 25% of the Entire code base. Yet strict prompting, a issues could be possible. please inform us in case of issues or broken logics founded. Thank you!

---

## Table of Contents

1. [Architecture & Storage Model](https://www.google.com/search?q=%23architecture--storage-model)
2. [Project Structure](https://www.google.com/search?q=%23project-structure)
3. [Building the Library](https://www.google.com/search?q=%23building-the-library)
4. [Quickstart Guide](https://www.google.com/search?q=%23quickstart-guide)
5. [API Reference](https://www.google.com/search?q=%23api-reference)
* [Types & Enums](https://www.google.com/search?q=%23types--enums)
* [Database Lifecycle & Persistence](https://www.google.com/search?q=%23database-lifecycle--persistence)
* [Table & Attribute DDL](https://www.google.com/search?q=%23table--attribute-ddl)
* [Record Operations](https://www.google.com/search?q=%23record-operations)
* [Selection & Results](https://www.google.com/search?q=%23selection--results)
* [Projections & Custom Printing](https://www.google.com/search?q=%23projections--custom-printing)


6. [Comprehensive Examples](https://www.google.com/search?q=%23comprehensive-examples)

---

## Architecture & Storage Model

Unlike traditional row-oriented relational databases, `cDB` organizes data contiguously by **Attribute** (Column) in memory to maximize data locality and cache efficiency for analytical queries.

### Terminology:

* **Attribute**: A column field featuring a unique name/alias, internal index, data size, type (`cDB_Datatype`), and validation state.
* **Record**: A database entry represented by a `uint64_t` handle (`cDB_Record`), mapping values across all table attributes.
* **Predicate**: A user-defined callback function (`cDB_Predicate`) used to filter records dynamically during selection operations.
* **Projection**: A subset view containing specified records and attributes for targeted analysis or custom rendering.

---

## Project Structure

```text
cDB/
├── CMakeLists.txt     # CMake build configuration
└── src/
    ├── cDB.h               # Public cDB high-level API & definitions
    ├── cDB.c               # Database engine, queries, and operations
    ├── cDB_Table.h         # Low-level columnar table API
    ├── cDB_Table.c         # Columnar memory layout & storage handlers
    ├── cDB_SpreadSheet.c   # Interactive TUI .cdb editor

```

---

## Building the Library

### Prerequisites

* C Compiler (`clang`, `gcc`, or MSVC)
* CMake (version 3.10+)

### Compile as Static Library (`.lib` / `.a`)

```bash
mkdir build
cd build
cmake ..
cmake --build .

```

This produces the compiled library binaries for your operating system.

---

## Quickstart Guide

```c
#include "cDB.h"
#include <stdio.h>

// Custom filter predicate: returns true if student score >= threshold
bool score_filter(cDB_Database* db, cDB_Record record, void* user_data) {
    cDB_Attribute score_attr = cDB_getAttribute(db, "score");
    double* score = (double*)cDB_getRecordValue(db, record, score_attr);
    double threshold = *(double*)user_data;
    return score && (*score >= threshold);
}

int main(void) {
    // 1. Initialize database instance
    cDB_Database* db = cDB_Init();

    // 2. Create a new table with max record and attribute limits
    cDB_newTable(db, "students", 100, 5);

    // 3. Define schema attributes
    cDB_createAttribute(db, "id", CDB_INT32, 0);
    cDB_createAttribute(db, "name", CDB_STRING, STD_CDB_STRLEN);
    cDB_createAttribute(db, "score", CDB_FLOAT64, 0);

    // 4. Retrieve a record handle and set values
    cDB_Record rec = cDB_getRecord(db, "student_101");
    int32_t id = 101;
    char name[STD_CDB_STRLEN] = "Alice";
    double score = 88.5;

    cDB_setRecordValue(db, rec, cDB_getAttribute(db, "id"), &id);
    cDB_setRecordValue(db, rec, cDB_getAttribute(db, "name"), name);
    cDB_setRecordValue(db, rec, cDB_getAttribute(db, "score"), &score);

    // 5. Query data using a predicate
    double min_score = 80.0;
    cDB_select(db, score_filter, &min_score);

    // 6. Project and print the result table formatted
    cDB_Projection* proj = cDB_getTableProjection(db);
    cDB_printProjection(db, proj, true, true, NULL, NULL, NULL);
    cDB_freeProjection(proj);

    // 7. Save to disk and close database
    cDB_Save(db, "school.cdb");
    cDB_Close(db);
    return 0;
}

```

---

## API Reference

### Types & Enums

#### `cDB_Datatype`

Supported primitive and custom attribute types:

| Type Constant | Description |
| --- | --- |
| `CDB_INT32` | 32-bit signed integer |
| `CDB_INT64` | 64-bit signed integer |
| `CDB_UINT32` | 32-bit unsigned integer |
| `CDB_UINT64` | 64-bit unsigned integer |
| `CDB_FLOAT32` | Single-precision floating point |
| `CDB_FLOAT64` | Double-precision floating point |
| `CDB_STRING` | Fixed-length string buffer |
| `CDB_CUSTOM` | Raw binary / custom size struct |

#### `cDB_SortDirection`

* `CDB_SORTDIR_ASCEND`: Sort ascending
* `CDB_SORTDIR_DESCEND`: Sort descending

#### `cDB_PrintStyle`

Defines custom borders and character layouts for ASCII grid printing (`UpperLeft_Corner`, `UpperRight_Corner`, `LowerLeft_Corner`, `LowerRight_Corner`, `VerticalLine`, `HorizontalLine`).

---

### Database Lifecycle & Persistence

#### `cDB_Database* cDB_Init(void)`

Initializes and returns a pointer to a fresh, empty `cDB_Database` instance.

#### `void cDB_Close(cDB_Database* db)`

Deallocates all internal table structures, query results, and frees the database instance.

#### `bool cDB_Save(cDB_Database* db, const char filename[65])`

Serializes the database schema, metadata, and records into a binary file. Returns `true` on success.

#### `bool cDB_Open(cDB_Database* db, const char filename[65])`

Opens and loads database state from a specified `.cdb` file. Returns `true` on success.

---

### Table & Attribute DDL

#### `bool cDB_newTable(cDB_Database* db, const char name[STD_CDB_STRLEN], const uint32_t maxRecord, const uint32_t maxAttribute)`

Creates a new table inside the database with defined upper limits for records and attributes.

#### `cDB_Attribute cDB_createAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN], cDB_Datatype type, size_t custom_size)`

Defines and creates a new attribute column on the active table schema.

#### `cDB_Attribute cDB_getAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN])`

Looks up and retrieves an attribute by its string alias/name.

#### `void cDB_setAttributeAlias(cDB_Database *db, cDB_Attribute attrib, const char alias[STD_CDB_STRLEN])`

Updates or assigns a string alias to an existing attribute.

---

### Record Operations

#### `cDB_Record cDB_getRecord(cDB_Database* db, const char alias[STD_CDB_STRLEN])`

Retrieves or instantiates a record handle by its unique record alias.

#### `void cDB_setRecordAlias(cDB_Database *db, cDB_Record record, const char alias[STD_CDB_STRLEN])`

Assigns a string alias to a given record handle.

#### `void cDB_setRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, void* data)`

Writes a typed data value into a record's specific attribute cell.

#### `void* cDB_getRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib)`

Returns a raw memory pointer to a record's value for the specified attribute.

---

### Selection & Results

#### `void cDB_clearResult(cDB_Database* db)`

Clears any existing query results cached in the database.

#### `bool cDB_select(cDB_Database* db, cDB_Predicate predicate, void* user_data)`

Executes a query by iterating over table records and evaluating the provided `cDB_Predicate` callback function with optional `user_data`.

#### `cDB_Record cDB_resultGetRecord(const cDB_Database* db, uint64_t result_idx)`

Retrieves a matching record handle from the result set by index.

#### `uint64_t cDB_resultLen(const cDB_Database* db)`

Returns the total number of records matching the latest selection query.

#### `void* cDB_resultGetValue(cDB_Database* db, uint64_t result_idx, cDB_Attribute attrib)`

Extracts a value directly from a result set entry for a given attribute.

---

### Projections & Custom Printing

#### `cDB_Projection* cDB_getProjection(cDB_Database* db, cDB_Record* records, uint64_t nRecords, const uint64_t* attrib_indices, uint64_t nAttributes)`

Constructs a custom projection view based on explicit record arrays and attribute index lists.

#### `cDB_Projection* cDB_getResultProjection(cDB_Database* db)`

Creates a projection view containing records from the current selection result set and all table attributes.

#### `cDB_Projection* cDB_getTableProjection(cDB_Database* db)`

Creates a full projection view encompassing all records and attributes in the active table.

#### `void cDB_freeProjection(cDB_Projection* proj)`

Frees allocated memory associated with a `cDB_Projection`.

#### `void cDB_printProjection(const cDB_Database* db, const cDB_Projection* proj, bool hSeperator, bool vSeperator, cDB_PrintStyle* printStyle, uint8_t* hPadding, uint8_t* vPadding)`

Renders and prints a projection grid formatted as an ASCII table to `stdout`.

---

## Comprehensive Examples

### 1. Creating Database, Tables, and Attributes

```c
#include "cDB.h"

int main(void) {
    cDB_Database* db = cDB_Init();

    // Create "employees" table with up to 500 records and 10 attributes
    cDB_newTable(db, "employees", 500, 10);

    // Define table attributes
    cDB_createAttribute(db, "id", CDB_INT32, 0);
    cDB_createAttribute(db, "name", CDB_STRING, 64);
    cDB_createAttribute(db, "department", CDB_STRING, 32);
    cDB_createAttribute(db, "salary", CDB_FLOAT64, 0);
    cDB_createAttribute(db, "active", CDB_INT32, 0);

    cDB_Close(db);
    return 0;
}

```

---

### 2. Inserting and Assigning Record Values

```c
#include "cDB.h"

void seed_employees(cDB_Database* db) {
    // Record 1
    cDB_Record r1 = cDB_getRecord(db, "emp_1");
    int32_t id1 = 1;
    char name1[65] = "Sarah Connor";
    char dept1[33] = "Security";
    double sal1 = 85000.0;
    int32_t act1 = 1;

    cDB_setRecordValue(db, r1, cDB_getAttribute(db, "id"), &id1);
    cDB_setRecordValue(db, r1, cDB_getAttribute(db, "name"), name1);
    cDB_setRecordValue(db, r1, cDB_getAttribute(db, "department"), dept1);
    cDB_setRecordValue(db, r1, cDB_getAttribute(db, "salary"), &sal1);
    cDB_setRecordValue(db, r1, cDB_getAttribute(db, "active"), &act1);

    // Record 2
    cDB_Record r2 = cDB_getRecord(db, "emp_2");
    int32_t id2 = 2;
    char name2[65] = "John Doe";
    char dept2[33] = "Engineering";
    double sal2 = 92000.0;
    int32_t act2 = 1;

    cDB_setRecordValue(db, r2, cDB_getAttribute(db, "id"), &id2);
    cDB_setRecordValue(db, r2, cDB_getAttribute(db, "name"), name2);
    cDB_setRecordValue(db, r2, cDB_getAttribute(db, "department"), dept2);
    cDB_setRecordValue(db, r2, cDB_getAttribute(db, "salary"), &sal2);
    cDB_setRecordValue(db, r2, cDB_getAttribute(db, "active"), &act2);
}

```

---

### 3. Querying with Custom Functional Predicates

```c
#include "cDB.h"
#include <string.h>

// Context struct to pass multiple criteria into the predicate function
typedef struct {
    const char* target_dept;
    double min_salary;
} QueryParams;

bool filter_department_and_salary(cDB_Database* db, cDB_Record record, void* user_data) {
    QueryParams* params = (QueryParams*)user_data;

    cDB_Attribute dept_attr = cDB_getAttribute(db, "department");
    cDB_Attribute sal_attr = cDB_getAttribute(db, "salary");

    char* dept = (char*)cDB_getRecordValue(db, record, dept_attr);
    double* salary = (double*)cDB_getRecordValue(db, record, sal_attr);

    bool match_dept = dept && (strcmp(dept, params->target_dept) == 0);
    bool match_sal = salary && (*salary >= params->min_salary);

    return match_dept && match_sal;
}

void run_query(cDB_Database* db) {
    QueryParams params = { .target_dept = "Engineering", .min_salary = 90000.0 };

    // Execute selection query via predicate callback
    cDB_select(db, filter_department_and_salary, &params);

    // Print result view projection
    cDB_Projection* proj = cDB_getResultProjection(db);
    cDB_printProjection(db, proj, true, true, NULL, NULL, NULL);
    cDB_freeProjection(proj);
}

```

---

### 4. Binary Persistence & Reloading

```c
#include "cDB.h"
#include <stdio.h>

int main(void) {
    // Session 1: Create, populate, and save
    cDB_Database* db1 = cDB_Init();
    cDB_newTable(db1, "inventory", 100, 3);
    // ... create attributes & insert records ...
    
    cDB_Save(db1, "inventory.cdb");
    cDB_Close(db1);

    // Session 2: Reload state from disk file
    cDB_Database* db2 = cDB_Init();
    if (cDB_Open(db2, "inventory.cdb")) {
        printf("Database successfully loaded from disk!\n");
        
        cDB_Projection* proj = cDB_getTableProjection(db2);
        cDB_printProjection(db2, proj, true, true, NULL, NULL, NULL);
        cDB_freeProjection(proj);
    }
    cDB_Close(db2);
    return 0;
}

```