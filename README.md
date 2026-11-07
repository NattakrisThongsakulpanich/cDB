# cDB Library

`cDB` is a lightweight, in-memory relational database management library written in C. It provides structured table creation, typed columns, array-based row insertion, conditional querying, updating, and row deletion.

This project is a part of KMUTT Department of Computer Engineering student's project. 

---

## Features

- **Database Lifecycle**: Simple API to initialize and close database instances (`cDB_open`, `cDB_close`).
- **Dynamic Schema Definition**: Create tables with custom column names, data types, string length limits, and capacity limits.
- **Supported Data Types**:
  - `CDB_TYPE_INT32`
  - `CDB_TYPE_DOUBLE`
  - `CDB_TYPE_STRING`
- **CRUD Operations**:
  - **Create**: Add rows using array pointers to field data.
  - **Read**: Perform `SELECT *` or filter rows using conditional operators (`CDB_OP_EQ`, `CDB_OP_GT`, etc.).
  - **Update**: Modify column values based on condition matching.
  - **Delete**: Remove records matching conditional criteria.
- **Built-in Utilities**: Easily output query result sets using `cDB_print_result` and manage memory with `cDB_free_result`.

---

## Project Structure

```text
cDB/
├── CMakeLists.txt
├── README.md
├── src/
    ├── cDB.c
    ├── cDB.h
    ├── cDB_Table.c
    ├── cDB_Table.h
    ├── safefOC.c
    └── safefOC.h

```

---

## Compiling from Source

### Prerequisites

- **CMake** (v3.10 or higher)
- any **C** compiler
- **Ninja** or **Make** build system

### 1. Build the Library

```bash
mkdir build
cd build
cmake .. -DCMAKE_C_COMPILER=YOUR_C_COMPILER
cmake --build .
```

This compiles `libcDB_Library.a` (or `cDB_Library.lib` on Windows) inside your `build` directory.

### 2. Including in a CMake Project

To use `cDB` in your own project, link against `cDB_Library` in your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyDatabaseApp C)

# Include the cDB library directory
add_subdirectory(cDB)

# Build executable
add_executable(app src/main.c)

# Link cDB static library
target_link_libraries(app PRIVATE cDB_Library)
```

---

## Usage & API Examples

### 1. Initializing and Closing Database

```c
#include "cDB.h"

// Open database with name and maximum table capacity
cDB_Database* db = cDB_open("MyEcommerceDB", 10);
if (!db) {
    // Handle allocation error
    return 1;
}

// ... execute operations ...

// Free database resources
cDB_close(db);
```

### 2. Creating a Table Schema

```c
cDB_ColumnDef user_cols[] = {
    {"id",      CDB_TYPE_INT32,  0},   // 32-bit Integer
    {"name",    CDB_TYPE_STRING, 64},  // Fixed-length String (max 64 chars)
    {"age",     CDB_TYPE_INT32,  0},   // 32-bit Integer
    {"balance", CDB_TYPE_DOUBLE, 0}    // Double Precision Float
};

// Parameters: database, table_name, columns, num_columns, max_capacity
cDB_create_table(db, "users", user_cols, 4, 100);
```

### 3. Inserting Data

Data is passed as an array of pointers to each column's value:

```c
int32_t id = 1;
char name[64] = "Alice";
int32_t age = 28;
double balance = 1250.50;

const void* row[] = { &id, name, &age, &balance };

// Parameters: database, table_name, record_key, row_data
cDB_insert(db, "users", "user_alice", row);
```

### 4. Querying Data (`SELECT`)

#### Select All Rows

```c
cDB_ResultSet* res_all = cDB_select(db, "users", NULL);

// Print query results to standard output
cDB_print_result(res_all);

// Clean up memory allocated for the result set
cDB_free_result(res_all);
```

#### Filtered Select (`WHERE age > 25`)

```c
int32_t filter_age = 25;

cDB_Condition cond_age = {
    .col_name = "age",
    .op = CDB_OP_GT,
    .val = &filter_age
};

cDB_ResultSet* res_filter = cDB_select(db, "users", &cond_age);
cDB_print_result(res_filter);
cDB_free_result(res_filter);
```

### 5. Updating Rows (`UPDATE`)

Update column values matching a condition:

```c
char target_name[64] = "Bob";
cDB_Condition cond_name = {
    .col_name = "name",
    .op = CDB_OP_EQ,
    .val = target_name
};

double new_balance = 1500.00;

// Parameters: database, table_name, target_column, new_value, condition
uint32_t updated_count = cDB_update(db, "users", "balance", &new_balance, &cond_name);
printf("Updated %u row(s).\n", updated_count);
```

### 6. Deleting Rows (`DELETE`)

Delete records matching a condition:

```c
int32_t delete_id = 3;
cDB_Condition cond_del = {
    .col_name = "id",
    .op = CDB_OP_EQ,
    .val = &delete_id
};

// Parameters: database, table_name, condition
uint32_t deleted_count = cDB_delete(db, "users", &cond_del);
printf("Deleted %u row(s).\n", deleted_count);
```