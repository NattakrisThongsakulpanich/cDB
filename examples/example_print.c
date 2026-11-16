#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cDB.h"

int main(void)
{
    printf("=== Example 3: Table & Projection Printing Showcase ===\n\n");

    cDB_Database* db = cDB_Init();
    cDB_newTable(db, "servers", 6, 4);

    cDB_Attribute host_attr   = cDB_createAttribute(db, "Hostname", CDB_STRING, 32);
    cDB_Attribute ip_attr     = cDB_createAttribute(db, "IP_Address", CDB_STRING, 20);
    cDB_Attribute cpu_attr    = cDB_createAttribute(db, "CPU_Load",  CDB_STRING, 12);
    cDB_Attribute status_attr = cDB_createAttribute(db, "Status",    CDB_STRING, 16);

    struct { const char* alias; const char* host; const char* ip; const char* cpu; const char* status; } nodes[] = {
        { "SRV_PRIMARY", "web-prod-01",  "10.0.1.10", "14.2%", "ONLINE" },
        { "SRV_BACKUP",  "web-prod-02",  "10.0.1.11", "88.7%", "HIGH_LOAD" },
        { "SRV_DB_MAIN", "db-cluster-01", "10.0.2.20", "42.0%", "ONLINE" },
        { "SRV_CACHE",   "redis-node-01", "10.0.3.30", "05.1%", "MAINTENANCE" }
    };

    for (size_t i = 0; i < sizeof(nodes)/sizeof(nodes[0]); i++) {
        cDB_setRecordAlias(db, i, nodes[i].alias);
        cDB_setRecordValue(db, i, host_attr,   (void*)nodes[i].host);
        cDB_setRecordValue(db, i, ip_attr,     (void*)nodes[i].ip);
        cDB_setRecordValue(db, i, cpu_attr,    (void*)nodes[i].cpu);
        cDB_setRecordValue(db, i, status_attr, (void*)nodes[i].status);
    }

    // -------------------------------------------------------------
    // Style 1: Default Style (Full Table, vertical borders, no row separator)
    // -------------------------------------------------------------
    printf("[1] Default Table Style (Full Table Projection):\n");
    cDB_Projection* full_table = cDB_getTableProjection(db);
    cDB_printProjection(db, full_table, false, true, NULL, NULL, NULL);

    // -------------------------------------------------------------
    // Style 2: Styled with Grid Horizontal Separators between rows
    // -------------------------------------------------------------
    printf("[2] Grid Style (Horizontal and Vertical Separators):\n");
    cDB_printProjection(db, full_table, true, true, NULL, NULL, NULL);

    // -------------------------------------------------------------
    // Style 3: Custom Print Style (ASCII Rounded/Dot style) with Horizontal Padding
    // -------------------------------------------------------------
    printf("[3] Custom Borders (/ \\) with Horizontal Padding = 2:\n");
    cDB_PrintStyle custom_style = {
        .UpperLeft_Corner  = '/',
        .UpperRight_Corner = '\\',
        .LowerLeft_Corner  = '\\',
        .LowerRight_Corner = '/',
        .VerticalLine      = '|',
        .HorizontalLine    = '='
    };
    uint8_t h_pad = 2;
    uint8_t v_pad = 0;
    cDB_printProjection(db, full_table, true, true, &custom_style, &h_pad, &v_pad);

    // -------------------------------------------------------------
    // Style 4: Selective Sub-Projection (Only Hostname and Status for specific records)
    // -------------------------------------------------------------
    printf("[4] Selective Column Projection (Hostname + Status only, Records: SRV_PRIMARY and SRV_CACHE):\n");
    cDB_Record subset_records[] = { 0, 3 };
    uint64_t subset_attrs[] = { host_attr.idx, status_attr.idx };

    cDB_Projection* sub_proj = cDB_getProjection(
        db,
        subset_records, 
        sizeof(subset_records)/sizeof(subset_records[0]),
        subset_attrs,
        sizeof(subset_attrs)/sizeof(subset_attrs[0])
    );

    if (sub_proj) {
        cDB_printProjection(db, sub_proj, false, true, NULL, NULL, NULL);
        cDB_freeProjection(sub_proj);
    }

    // -------------------------------------------------------------
    // Style 5: Borderless / Minimalist (vSeperator = false, hSeperator = false)
    // -------------------------------------------------------------
    printf("[5] Borderless View (No vertical lines):\n");
    cDB_printProjection(db, full_table, false, false, NULL, NULL, NULL);

    cDB_freeProjection(full_table);
    cDB_Close(db);
    return 0;
}
