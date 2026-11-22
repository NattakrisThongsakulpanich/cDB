# cDB

`cDB` is a small, embedded, column-oriented database library written in C99. It is a learning and technology-demonstration project inspired by a few SQLite-like ideas: an application owns a database object, defines a schema, writes records, queries them, and optionally saves them to a file.

It is deliberately narrow in scope. A `cDB_Database` owns **one table at a time**. cDB is not an SQL engine and does not implement joins, transactions, indexes, concurrency, or multi-table relationships.

This project is developed as a final project of the KMUTT Department of Computer Engineering, Computer Programming 101 (2026).  

NOTICE: Due to a tight project deadline and project scale. This project is AI assisted approximately to 40% of the Entire code base. Yet strict prompting, a issues could be possible. please inform us in case of issues or broken logics founded. Thank you!

## Features

- Column-oriented in-memory storage: each attribute owns one contiguous value block.
- Fixed capacities selected when the table is created.
- Typed attributes: booleans, integers, floating-point values, strings, and custom binary values.
- Record and attribute names for readable lookup and display.
- Predicate-based selection through a regular C callback.
- Projections for table views, query results, or selected records/attributes.
- Console-table printing and RFC 4180 CSV export.
- Simple binary `.cdb` persistence and a command-line viewer/exporter.

## Project layout

```text
cDB/
├── include/
│   ├── cDB.h             # Public database API
│   └── cDB_Table.h       # Low-level column-storage API
├── src/
│   ├── cDB.c             # Database, queries, persistence, projections
│   ├── cDB_Table.c       # Column-oriented table storage
│   └── all_cDB_TUI.c     # cdb command-line utility
├── examples/
│   └── example1.c        # Create, save, reopen, and print a table
└── CMakeLists.txt
```

## Build

Requirements: CMake 3.10+ and a C99 compiler (MSVC, Clang, or GCC).

```sh
cmake -S . -B build
cmake --build build
```

The build produces the `cDB` static library, the `cdb` command-line utility, and `example1`.

## Data model

- **Database** — owns one active table, its type metadata, and the most recent query result.
- **Table** — fixed-capacity, column-oriented storage.
- **Attribute** — a typed column identified by a name and an index.
- **Record** — a `uint64_t` slot index. A record becomes active when it has a non-empty record name.

`recordCapacity` and `attributeCapacity` are upper bounds, not current counts. A table is not resized after it is created.

## Quick start

This creates an inventory table, fills one record, prints it, and saves it as `inventory.cdb`.

```c
#include "cDB.h"
#include <stdint.h>

int main(void)
{
    cDB_Database *db = cdb_Create();
    if (!db) return 1;

    if (!cdb_CreateTable(db, "inventory", 100, 3)) {
        cdb_Destroy(db);
        return 1;
    }

    cDB_Attribute item = cdb_CreateAttribute(db, "item", CDB_STRING, 32);
    cDB_Attribute stock = cdb_CreateAttribute(db, "stock", CDB_UINT32, 0);
    cDB_Attribute price = cdb_CreateAttribute(db, "price", CDB_FLOAT32, 0);

    cdb_set_recordName(db, 0, "keyboard");
    cdb_set_string(db, 0, &item, "Mechanical Keyboard");
    cdb_set_u32(db, 0, &stock, 45);
    cdb_set_f32(db, 0, &price, 99.99f);

    cDB_Projection *table = cdb_CreateTableProjection(db);
    cdb_printProjection(db, table, false, true, NULL);
    cdb_freeProjection(table);

    cdb_save(db, "inventory.cdb");
    cdb_Destroy(db);
    return 0;
}
```

`cdb_Destroy()` saves an unsaved active table before freeing it. Call `cdb_save()` explicitly when you need to choose when or where the file is written.

## Types

| Constant | Stored value |
| --- | --- |
| `CDB_CUSTOM` | Raw binary data with a caller-provided size |
| `CDB_BOOL` | `bool` |
| `CDB_INT32` / `CDB_INT64` | Signed 32- / 64-bit integer |
| `CDB_UINT32` / `CDB_UINT64` | Unsigned 32- / 64-bit integer |
| `CDB_FLOAT32` / `CDB_FLOAT64` | `float` / `double` |
| `CDB_STRING` | Fixed-size, null-terminated character buffer |

Pass `0` as `custom_size` for built-in types. For `CDB_STRING`, it is each buffer’s capacity, including the null terminator. For `CDB_CUSTOM`, it is the number of bytes in each cell.

## .cdb File Architecture

| Order | Field | Size | Description |
|---:|---|---:|---|
| 0 | Magic bytes | 12 bytes | File identifier: `de1whoasked\0` |
| 1 | Table name | 65 bytes | Null-terminated table name buffer |
| 2 | Attribute capacity | 8 bytes | Maximum number of attributes (`uint64_t`) |
| 3 | Record capacity | 8 bytes | Maximum number of records (`uint64_t`) |
| 4 | Attribute sizes | `attributeCapacity × sizeof(size_t)` | Size in bytes of one value for each attribute |
| 5 | Attribute names | `attributeCapacity × 65 bytes` | Fixed-size name buffer for every attribute slot |
| 6 | Record names | `recordCapacity × 65 bytes` | Fixed-size name buffer for every record slot |
| 7 | Column data | Variable | One contiguous data block per attribute, written in attribute-index order |
| 8 | Attribute type references | `attributeCapacity × sizeof(cDB_Datatype)` | The `cDB_Datatype` value for each attribute |

## Public API

[`include/cDB.h`](include/cDB.h) is authoritative.

### Lifecycle and persistence

```c
cDB_Database *cdb_Create(void);
void cdb_Destroy(cDB_Database *db);

bool cdb_CreateTable(
    cDB_Database *db,
    const char name[CDB_STD_STRLEN],
    uint32_t recordCapacity,
    uint32_t attributeCapacity
);

bool cdb_save(cDB_Database *db, const char filename[65]);
bool cdb_open(cDB_Database *db, const char filename[65]);
bool cdb_exportCSV(cDB_Database *db, const cDB_Projection *projection,
                   const char filename[65]);
```

Pass `NULL` to `cdb_save()` to save as `<table-name>.cdb`. `cdb_open()` save the current active table and open new table of the associated filename. The `.cdb` format is a simple native binary format intended for compatible cDB builds, not portable interchange.

### Schema and record names

```c
cDB_Attribute cdb_CreateAttribute(cDB_Database *db, const char name[CDB_STD_STRLEN], cDB_Datatype type, size_t custom_size);
cDB_Attribute cdb_getAttribute(cDB_Database *db, const char name[CDB_STD_STRLEN]);
cDB_Attribute cdb_getAttribute_byIndex(cDB_Database *db, uint64_t index);
void cdb_setAttributeName(cDB_Database *db, cDB_Attribute *attribute, const char name[CDB_STD_STRLEN]);
cDB_Record cdb_get_record(cDB_Database *db, const char name[CDB_STD_STRLEN]);
void cdb_set_recordName(cDB_Database *db, cDB_Record record, const char name[CDB_STD_STRLEN]);
```

`cdb_get_record()` looks up an existing record name and returns `UINT64_MAX` when it cannot find one. It does not create a record. Select a valid slot below `recordCapacity`, then activate it with `cdb_set_recordName()`.

### Values

For raw or custom values:

```c
void cdb_set_t(cDB_Database *db, cDB_Record record, cDB_Attribute *attribute, const void *data);
void *cdb_get_t(cDB_Database *db, cDB_Record record, cDB_Attribute *attribute);
```

For built-in types, prefer the typed functions:

```c
cdb_set_bool(...)    cdb_get_bool(...)
cdb_set_i32(...)     cdb_get_i32(...)
cdb_set_i64(...)     cdb_get_i64(...)
cdb_set_u32(...)     cdb_get_u32(...)
cdb_set_u64(...)     cdb_get_u64(...)
cdb_set_f32(...)     cdb_get_f32(...)
cdb_set_f64(...)     cdb_get_f64(...)
cdb_set_string(...)  cdb_get_string(...)
```

Each typed function takes `db`, `record`, and `cDB_Attribute *`; setters then take the new value. `cdb_get_t()` returns a pointer into cDB-managed memory: do not free it or retain it after replacing/destroying the table.

### Selection

Selections evaluate a C callback for every active record:

```c
typedef bool (*cDB_Predicate)(cDB_Database *db, cDB_Record record, void *user_data);

bool cdb_select(cDB_Database *db, cDB_Predicate predicate, void *user_data);
void cdb_clearResult(cDB_Database *db);
const cDB_Result *cdb_getResult(const cDB_Database *db);
```

Example: select items with stock below a limit.

```c
static bool low_stock(cDB_Database *db, cDB_Record record, void *user_data)
{
    uint32_t limit = *(uint32_t *)user_data;
    cDB_Attribute stock = cdb_getAttribute(db, "stock");
    return cdb_get_u32(db, record, &stock) < limit;
}

uint32_t limit = 20;
cdb_select(db, low_stock, &limit);
```

Pass `NULL` as the predicate to select all active records. The result belongs to the database and is replaced by the next selection, table replacement, or destruction.

### Projections and printing

```c
cDB_Projection *cdb_CreateProjection(
    cDB_Database *db, 
    cDB_Record *records, 
    uint64_t recordCount,
    uint64_t attributeCount,
    const uint64_t *attributeIndices
);
cDB_Projection *cdb_CreateResultProjection(cDB_Database *db);
cDB_Projection *cdb_CreateTableProjection(cDB_Database *db);
void cdb_freeProjection(cDB_Projection *projection);

void cdb_printProjection(
    cDB_Database *db, 
    cDB_Projection *projection,
    bool horizontalSeparator, bool verticalSeparator, 
    cDB_PrintStyle *printStyle
);
```

- `cdb_CreateTableProjection()` includes every active record and attribute.
- `cdb_CreateResultProjection()` uses records from the most recent selection.
- `cdb_CreateProjection()` creates a chosen view. Pass `NULL` for either index array to use sequential indices in that dimension.
- Free every successful projection with `cdb_freeProjection()`.

`cdb_printProjection()` writes an ASCII table to standard output. Pass `NULL` for `printStyle` to use the default `+`, `-`, and `|` border style.

## Command-line utility

After building, use `cdb` to inspect a saved database or export it to CSV:

```sh
cdb inventory.cdb
cdb --view-t inventory.cdb
cdb --export-csv inventory.cdb inventory.csv
```

## Limitations and intended use

cDB is built for coursework, experiments, and small embedded demonstrations. It does not provide SQL parsing, multiple tables in a database object, joins, indexes, transactions, concurrency control, dynamic table resizing, row deletion, or cross-platform/versioned binary compatibility.

For production work, use a mature database system. cDB is intended to make columnar layout, C callbacks, binary serialization, and API design easy to inspect and learn from.
