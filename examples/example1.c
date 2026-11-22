#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/cDB.h"

int main(void)
{
    printf("=== Example 1: Save and Open Mechanism ===\n\n");

    // 1. Initialize a new database instance
    cDB_Database* db = cdb_Create();
    if (!db) {
        printf("Failed to initialize database.\n");
        return 1;
    }

    // 2. Create a new table named "inventory" (max 5 records, max 3 attributes)
    if (!cdb_CreateTable(db, "inventory", 5, 3)) {
        printf("Failed to create table.\n");
        cdb_Destroy(db);
        return 1;
    }
    printf("[1] Created table 'inventory' (recordCapacity=5, attributeCapacity=3).\n");

    // 3. Define attributes
    cDB_Attribute item_attr  = cdb_CreateAttribute(db, "ItemName", CDB_STRING, 32);
    cDB_Attribute stock_attr = cdb_CreateAttribute(db, "Stock",    CDB_UINT32, NULL);
    cDB_Attribute price_attr = cdb_CreateAttribute(db, "Price",    CDB_FLOAT32, NULL);

    // 4. Populate rows with records and values
    // Row 0
    cdb_set_recordName(db, 0, "INV_001");
    cdb_set_string(db, 0, &item_attr, "Mechanical Keyboard");
    cdb_set_u32(db, 0, &stock_attr, 45);
    cdb_set_f32(db, 0, &price_attr, 99.99);

    // Row 1
    cdb_set_recordName(db, 1, "INV_002");
    cdb_set_string(db, 1, &item_attr, "Wireless Mouse");
    cdb_set_u32(db, 1, &stock_attr, 120);
    cdb_set_f32(db, 1, &price_attr, 49.50);

    // Row 2
    cdb_set_recordName(db, 2, "INV_003");
    cdb_set_string(db, 2, &item_attr, "4K Monitor");
    cdb_set_u32(db, 2, &stock_attr, 18);
    cdb_set_f32(db, 2, &price_attr, 349.00);

    printf("[2] Added 3 inventory records into active session.\n");

    // 5. Save table to disk (creates "inventory.cdb" with magic byte header)
    if (cdb_save(db, NULL)) {
        printf("[3] Table successfully saved to 'inventory.cdb'.\n");
    } else {
        printf("Failed to save database.\n");
    }

    // Close and free current database session
    cdb_Destroy(db);
    printf("[4] Closed database session and freed memory.\n\n");

    // 6. Reopen the database from file in a fresh session
    cDB_Database* db2 = cdb_Create();
    printf("[5] Opening 'inventory.cdb' in a fresh database session...\n");
    if (!cdb_open(db2, "inventory.cdb")) {
        printf("Failed to open 'inventory.cdb'.\n");
        cdb_Destroy(db2);
        return 1;
    }
    printf("[6] Successfully loaded 'inventory.cdb'!\n\n");

    // 7. Verify loaded contents via projection
    cDB_Projection* proj = cdb_CreateTableProjection(db2);
    if (proj) {
        printf("Loaded Table View:\n");
        cdb_printProjection(db2, proj, false, true, NULL);
        cdb_freeProjection(proj);
    }

    cdb_Destroy(db2);
    printf("Session closed cleanly.\n");
    return 0;
}
