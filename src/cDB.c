#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cDB.h"
#include "./safefOC.h"
#include "cDB_Table.h"

const char MAGIC_BYTE[12] = {0x64, 0x65, 0x31, 0x77, 0x68, 0x6f, 0x61, 0x73, 0x6b, 0x65, 0x64, '\0'};

static size_t get_type_size(cDB_DataType type, size_t custom_size) {
    switch (type) {
        case CDB_TYPE_INT32:  return sizeof(int32_t);
        case CDB_TYPE_INT64:  return sizeof(int64_t);
        case CDB_TYPE_FLOAT:  return sizeof(float);
        case CDB_TYPE_DOUBLE: return sizeof(double);
        case CDB_TYPE_STRING: return custom_size > 0 ? custom_size : 64;
        case CDB_TYPE_BYTES:  return custom_size > 0 ? custom_size : 8;
        default: return 8;
    }
}

cDB_Database* cDB_open(const char* name, uint32_t max_tables) {

    if (!name || max_tables == 0) return NULL;

    cDB_Database* db = (cDB_Database*)calloc(1, sizeof(cDB_Database));
    if (!db) return NULL;

    char filename[128];
    snprintf(filename, sizeof(filename), "%s.cdb", name);

    FILE* cdbFile = safe_fopen(filename, "rb");
    if (!cdbFile) {
        // File does not exist yet -> initialize fresh database
        goto FRESH_ALLOCATION;
    }

    // MagicByte check
    char magicbyte[sizeof(MAGIC_BYTE)];
    if (fread(magicbyte, sizeof(char), sizeof(MAGIC_BYTE), cdbFile) != sizeof(MAGIC_BYTE) ||
        memcmp(magicbyte, MAGIC_BYTE, sizeof(MAGIC_BYTE)) != 0) {
        safe_fclose(cdbFile, filename);
        goto FRESH_ALLOCATION;
    }

    // Reads Metadata (matches cDB_close: char[64], uint32_t, uint32_t)
    if (fread(db->name, sizeof(char), 64, cdbFile) != 64 ||
        fread(&db->max_tables, sizeof(uint32_t), 1, cdbFile) != 1 ||
        fread(&db->table_count, sizeof(uint32_t), 1, cdbFile) != 1) {
        safe_fclose(cdbFile, filename);
        goto FRESH_ALLOCATION;
    }

    // Validate table limits
    if (db->max_tables == 0 || db->table_count > db->max_tables) {
        safe_fclose(cdbFile, filename);
        goto FRESH_ALLOCATION;
    }

    // Ensure capacity accommodates at least the requested max_tables or stored max_tables
    if (max_tables > db->max_tables) {
        db->max_tables = max_tables;
    }

    db->table_names = (char(*)[64])calloc(db->max_tables, sizeof(char[64]));
    db->tables = (cDB_Table**)calloc(db->max_tables, sizeof(cDB_Table*));    
    db->attr_types = (cDB_DataType**)calloc(db->max_tables, sizeof(cDB_DataType*));

    if (!db->table_names || !db->tables || !db->attr_types) {
        safe_fclose(cdbFile, filename);
        cDB_close(db);
        return NULL;
    }

    for (uint32_t i = 0; i < db->table_count; i++)
    {
        uint32_t num_attrs = 0;
        if (fread(db->table_names[i], sizeof(char), 64, cdbFile) != 64 ||
            fread(&num_attrs, sizeof(uint32_t), 1, cdbFile) != 1) {
            safe_fclose(cdbFile, filename);
            cDB_close(db);
            return NULL;
        }

        if (num_attrs > 0) {
            db->attr_types[i] = (cDB_DataType*)calloc(num_attrs, sizeof(cDB_DataType));
            if (!db->attr_types[i] ||
                fread(db->attr_types[i], sizeof(cDB_DataType), num_attrs, cdbFile) != num_attrs) {
                safe_fclose(cdbFile, filename);
                cDB_close(db);
                return NULL;
            }

            // Peek maxAttribute and maxRecord header from file to allocate table
            uint32_t maxAttr = 0, maxRec = 0;
            long header_pos = ftell(cdbFile);
            if (fread(&maxAttr, sizeof(uint32_t), 1, cdbFile) != 1 ||
                fread(&maxRec, sizeof(uint32_t), 1, cdbFile) != 1) {
                safe_fclose(cdbFile, filename);
                cDB_close(db);
                return NULL;
            }
            fseek(cdbFile, header_pos, SEEK_SET);

            db->tables[i] = cDB_Table_init(maxAttr, maxRec);
            if (!db->tables[i] || !cDB_Table_fread(db->tables[i], cdbFile)) {
                safe_fclose(cdbFile, filename);
                cDB_close(db);
                return NULL;
            }
        }
    }

    safe_fclose(cdbFile, filename);
    return db;

FRESH_ALLOCATION:

    strncpy(db->name, name, sizeof(db->name) - 1);
    db->max_tables = max_tables;
    db->table_count = 0;
    db->table_names = (char(*)[64])calloc(max_tables, sizeof(char[64]));
    db->tables = (cDB_Table**)calloc(max_tables, sizeof(cDB_Table*));    
    db->attr_types = (cDB_DataType**)calloc(max_tables, sizeof(cDB_DataType*));

    if (!db->table_names || !db->tables || !db->attr_types) {
        cDB_close(db);
        return NULL;
    }
    return db;
}

void cDB_close(cDB_Database* db) {

    if (!db) return;

    // Write database metadata & tables to file if database name is valid
    if (db->name[0] != '\0') {
        char filename[128];
        snprintf(filename, sizeof(filename), "%s.cdb", db->name);

        FILE* cdbFile = safe_fopen(filename, "wb");
        if (cdbFile != NULL) {
            // 1. Write DB Header:MagicBytes: de1whoasked, Database Name (fixed 64 bytes), max_tables, table_count
            fwrite(MAGIC_BYTE, sizeof(MAGIC_BYTE), 1, cdbFile);
            fwrite(db->name, sizeof(char), 64, cdbFile);
            fwrite(&db->max_tables, sizeof(uint32_t), 1, cdbFile);
            fwrite(&db->table_count, sizeof(uint32_t), 1, cdbFile);

            // 2. Write each table's schema and attribute content
            for (uint32_t i = 0; i < db->table_count; i++) {
                // Table Name (fixed 64 bytes)
                fwrite(db->table_names[i], sizeof(char), 64, cdbFile);

                cDB_Table* t = db->tables[i];
                if (t != NULL) {
                    // Attribute data types array
                    uint32_t num_attrs = t->maxAttribute;
                    fwrite(&num_attrs, sizeof(uint32_t), 1, cdbFile);
                    if (db->attr_types[i] != NULL && num_attrs > 0) {
                        fwrite(db->attr_types[i], sizeof(cDB_DataType), num_attrs, cdbFile);
                    }

                    // Write table payload via cDB_Table_fwrite
                    cDB_Table_fwrite(t, cdbFile);
                } else {
                    // Empty/Null table placeholder
                    uint32_t num_attrs = 0;
                    fwrite(&num_attrs, sizeof(uint32_t), 1, cdbFile);
                }
            }

            safe_fclose(cdbFile, filename);
        } else {
            fprintf(stderr, "[cDB_close] Warning: Failed to open '%s' for saving.\n", filename);
        }
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        if (db->tables[i]) {
            cDB_Table_destroy(db->tables[i]);
            db->tables[i] = NULL;
        }
        if (db->attr_types[i]) {
            free(db->attr_types[i]);
            db->attr_types[i] = NULL;
        }
    }
    free(db->table_names);
    free(db->tables);
    free(db->attr_types);
    free(db);
}

static int32_t find_table_idx(const cDB_Database* db, const char* table_name) {
    if (!db || !table_name) return -1;
    for (uint32_t i = 0; i < db->table_count; i++) {
        if (strcmp(db->table_names[i], table_name) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

bool cDB_create_table
(
    cDB_Database* db, 
    const char* table_name, 
    const cDB_AttributeDef* attrs, 
    uint32_t num_attrs, 
    uint32_t max_records
)
{
    if (!db || !table_name || !attrs || num_attrs == 0 || max_records == 0) return false;
    if (db->table_count >= db->max_tables) return false;
    if (find_table_idx(db, table_name) >= 0) return false; // Table already exists

    uint32_t tidx = db->table_count;
    cDB_Table* t = cDB_Table_init(num_attrs, max_records);
    if (!t) return false;

    cDB_DataType* types = (cDB_DataType*)calloc(num_attrs, sizeof(cDB_DataType));
    if (!types) {
        cDB_Table_destroy(t);
        return false;
    }

    for (uint32_t i = 0; i < num_attrs; i++) {
        cDB_Table_setAttributeAlias(t, i, attrs[i].name);
        size_t sz = get_type_size(attrs[i].type, attrs[i].size);
        cDB_Table_setAttributeSize(t, i, sz);
        types[i] = attrs[i].type;
    }

    strncpy(db->table_names[tidx], table_name, sizeof(db->table_names[tidx]) - 1);
    db->tables[tidx] = t;
    db->attr_types[tidx] = types;
    db->table_count++;

    return true;
}

bool cDB_drop_table(cDB_Database* db, const char* table_name) {
    int32_t idx = find_table_idx(db, table_name);
    if (idx < 0) return false;

    uint32_t tidx = (uint32_t)idx;
    if (db->tables[tidx]) cDB_Table_destroy(db->tables[tidx]);
    if (db->attr_types[tidx]) free(db->attr_types[tidx]);

    // Shift remaining tables
    for (uint32_t i = tidx; i + 1 < db->table_count; i++) {
        memcpy(db->table_names[i], db->table_names[i + 1], sizeof(char[64]));
        db->tables[i] = db->tables[i + 1];
        db->attr_types[i] = db->attr_types[i + 1];
    }
    db->table_count--;
    return true;
}

int64_t cDB_insert(cDB_Database* db, const char* table_name, const char* record_alias, const void** values) {
    int32_t idx = find_table_idx(db, table_name);
    if (idx < 0 || !values) return -1;

    cDB_Table* t = db->tables[idx];

    // Find the next available record slot
    uint32_t target_record = UINT32_MAX;
    for (uint32_t r = 0; r < t->maxRecord; r++) {
        if (t->RecordAlias[r][0] == '\0') {
            target_record = r;
            break;
        }
    }

    if (target_record == UINT32_MAX) return -1; // Table full

    // Set record alias if provided, or default to record index
    if (record_alias && record_alias[0] != '\0') {
        cDB_Table_setRecordAlias(t, target_record, record_alias);
    } else {
        char auto_alias[64];
        snprintf(auto_alias, sizeof(auto_alias), "record_%u", target_record);
        cDB_Table_setRecordAlias(t, target_record, auto_alias);
    }

    // Set attribute values
    for (uint32_t c = 0; c < t->maxAttribute; c++) {
        if (values[c] != NULL) {
            cDB_Table_setRecord(t, target_record, c, (void*)values[c]);
        }
    }

    return (int64_t)target_record;
}

static int compare_values(const void* record_val, const void* cond_val, cDB_DataType type) {
    if (!record_val && !cond_val) return 0;
    if (!record_val) return -1;
    if (!cond_val) return 1;

    switch (type) {
        case CDB_TYPE_INT32: {
            int32_t a = *(const int32_t*)record_val;
            int32_t b = *(const int32_t*)cond_val;
            return (a > b) - (a < b);
        }
        case CDB_TYPE_INT64: {
            int64_t a = *(const int64_t*)record_val;
            int64_t b = *(const int64_t*)cond_val;
            return (a > b) - (a < b);
        }
        case CDB_TYPE_FLOAT: {
            float a = *(const float*)record_val;
            float b = *(const float*)cond_val;
            return (a > b) - (a < b);
        }
        case CDB_TYPE_DOUBLE: {
            double a = *(const double*)record_val;
            double b = *(const double*)cond_val;
            return (a > b) - (a < b);
        }
        case CDB_TYPE_STRING: {
            return strcmp((const char*)record_val, (const char*)cond_val);
        }
        default:
            return 0;
    }
}

static bool check_condition(cDB_Table* t, cDB_DataType* types, uint32_t record_idx, const cDB_Condition* cond) {
    if (!cond) return true; // No WHERE clause matches all

    uint32_t attr_idx = cDB_Table_getAttributeIdx(t, cond->attr_name);
    if (attr_idx == UINT32_MAX) return false;

    void* val_ptr = cDB_Table_getRecord(t, record_idx, attr_idx);
    if (!val_ptr) return false;

    int cmp = compare_values(val_ptr, cond->val, types[attr_idx]);

    switch (cond->op) {
        case CDB_OP_EQ:  return cmp == 0;
        case CDB_OP_NEQ: return cmp != 0;
        case CDB_OP_LT:  return cmp < 0;
        case CDB_OP_LTE: return cmp <= 0;
        case CDB_OP_GT:  return cmp > 0;
        case CDB_OP_GTE: return cmp >= 0;
        default: return false;
    }
}

cDB_ResultSet* cDB_select(cDB_Database* db, const char* table_name, const cDB_Condition* cond) {
    int32_t idx = find_table_idx(db, table_name);
    if (idx < 0) return NULL;

    uint32_t tidx = (uint32_t)idx;
    cDB_Table* t = db->tables[tidx];

    cDB_ResultSet* res = (cDB_ResultSet*)calloc(1, sizeof(cDB_ResultSet));
    if (!res) return NULL;

    res->db = db;
    res->table_idx = tidx;
    res->capacity = 16;
    res->record_indices = (uint32_t*)malloc(res->capacity * sizeof(uint32_t));
    res->count = 0;

    for (uint32_t r = 0; r < t->maxRecord; r++) {
        // Only active/named records
        if (t->RecordAlias[r][0] == '\0') continue;

        if (check_condition(t, db->attr_types[tidx], r, cond)) {
            if (res->count >= res->capacity) {
                res->capacity *= 2;
                res->record_indices = (uint32_t*)realloc(res->record_indices, res->capacity * sizeof(uint32_t));
            }
            res->record_indices[res->count++] = r;
        }
    }

    return res;
}

void cDB_free_result(cDB_ResultSet* res) {
    if (!res) return;
    if (res->record_indices) free(res->record_indices);
    free(res);
}

const void* cDB_result_get(const cDB_ResultSet* res, uint32_t result_record_idx, const char* attr_name) {
    if (!res || result_record_idx >= res->count) return NULL;
    cDB_Table* t = res->db->tables[res->table_idx];
    uint32_t attr_idx = cDB_Table_getAttributeIdx(t, attr_name);
    if (attr_idx == UINT32_MAX) return NULL;

    uint32_t actual_record = res->record_indices[result_record_idx];
    return cDB_Table_getRecord(t, actual_record, attr_idx);
}

int32_t cDB_result_get_int32(const cDB_ResultSet* res, uint32_t result_record_idx, const char* attr_name, int32_t default_val) {
    const void* ptr = cDB_result_get(res, result_record_idx, attr_name);
    return ptr ? *(const int32_t*)ptr : default_val;
}

int64_t cDB_result_get_int64(const cDB_ResultSet* res, uint32_t result_record_idx, const char* attr_name, int64_t default_val) {
    const void* ptr = cDB_result_get(res, result_record_idx, attr_name);
    return ptr ? *(const int64_t*)ptr : default_val;
}

double cDB_result_get_double(const cDB_ResultSet* res, uint32_t result_record_idx, const char* attr_name, double default_val) {
    const void* ptr = cDB_result_get(res, result_record_idx, attr_name);
    return ptr ? *(const double*)ptr : default_val;
}

const char* cDB_result_get_string(const cDB_ResultSet* res, uint32_t result_record_idx, const char* attr_name) {
    const void* ptr = cDB_result_get(res, result_record_idx, attr_name);
    return ptr ? (const char*)ptr : "";
}

uint32_t cDB_update(cDB_Database* db, const char* table_name, const char* target_attr, const void* new_val, const cDB_Condition* cond) {
    int32_t idx = find_table_idx(db, table_name);
    if (idx < 0 || !target_attr || !new_val) return 0;

    uint32_t tidx = (uint32_t)idx;
    cDB_Table* t = db->tables[tidx];
    uint32_t attr_idx = cDB_Table_getAttributeIdx(t, target_attr);
    if (attr_idx == UINT32_MAX) return 0;

    uint32_t updated_count = 0;
    for (uint32_t r = 0; r < t->maxRecord; r++) {
        if (t->RecordAlias[r][0] == '\0') continue;

        if (check_condition(t, db->attr_types[tidx], r, cond)) {
            cDB_Table_setRecord(t, r, attr_idx, (void*)new_val);
            updated_count++;
        }
    }
    return updated_count;
}

uint32_t cDB_delete(cDB_Database* db, const char* table_name, const cDB_Condition* cond) {
    int32_t idx = find_table_idx(db, table_name);
    if (idx < 0) return 0;

    uint32_t tidx = (uint32_t)idx;
    cDB_Table* t = db->tables[tidx];

    uint32_t deleted_count = 0;
    for (uint32_t r = 0; r < t->maxRecord; r++) {
        if (t->RecordAlias[r][0] == '\0') continue;

        if (check_condition(t, db->attr_types[tidx], r, cond)) {
            // Clear alias
            t->RecordAlias[r][0] = '\0';
            // Clear data in all attributes for this record
            for (uint32_t c = 0; c < t->maxAttribute; c++) {
                void* rec = cDB_Table_getRecord(t, r, c);
                if (rec) {
                    memset(rec, 0, t->AttributeSizes[c]);
                }
            }
            deleted_count++;
        }
    }
    return deleted_count;
}

void cDB_print_result(const cDB_ResultSet* res) {
    if (!res) {
        printf("(NULL Result Set)\n");
        return;
    }

    cDB_Table* t = res->db->tables[res->table_idx];
    cDB_DataType* types = res->db->attr_types[res->table_idx];

    printf("+-------------------------------------------------------------------------+\n");
    printf("| Rec | Alias                | ");
    for (uint32_t c = 0; c < t->maxAttribute; c++) {
        printf("%-15s | ", t->AttributeAlias[c]);
    }
    printf("\n+-------------------------------------------------------------------------+\n");

    for (uint32_t i = 0; i < res->count; i++) {
        uint32_t r = res->record_indices[i];
        printf("| %-3u | %-20s | ", r, t->RecordAlias[r]);

        for (uint32_t c = 0; c < t->maxAttribute; c++) {
            void* val = cDB_Table_getRecord(t, r, c);
            if (!val) {
                printf("%-15s | ", "NULL");
                continue;
            }

            switch (types[c]) {
                case CDB_TYPE_INT32:
                    printf("%-15d | ", *(int32_t*)val);
                    break;
                case CDB_TYPE_INT64:
                    printf("%-15lld | ", (long long)*(int64_t*)val);
                    break;
                case CDB_TYPE_FLOAT:
                    printf("%-15.2f | ", *(float*)val);
                    break;
                case CDB_TYPE_DOUBLE:
                    printf("%-15.2f | ", *(double*)val);
                    break;
                case CDB_TYPE_STRING:
                    printf("%-15s | ", (char*)val);
                    break;
                default:
                    printf("%-15s | ", "[BINARY]");
                    break;
            }
        }
        printf("\n");
    }
    printf("+-------------------------------------------------------------------------+\n");
    printf("(%u records returned)\n\n", res->count);
}
