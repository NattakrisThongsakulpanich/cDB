#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "../include/cDB.h"

int main(void)
{
	printf("=== Testing cdb_importCSV with Explicit Schema ===\n");

	cDB_Database* db = cdb_Create();
	assert(db != NULL);

	// Schema for inventory.csv:
	// Col 0: ItemName -> CDB_STRING
	// Col 1: Stock    -> CDB_UINT32
	// Col 2: Price    -> CDB_FLOAT32
	cDB_Datatype format[] = { CDB_STRING, CDB_UINT32, CDB_FLOAT32 };

	bool ok = cdb_importCSV(db, "inventory.csv", format);
	if (!ok) {
		printf("FAIL: cdb_importCSV returned false\n");
		cdb_Destroy(db);
		return 1;
	}

	printf("PASS: Successfully imported inventory.csv\n");

	// Verify table projection
	cDB_Projection* proj = cdb_CreateTableProjection(db);
	assert(proj != NULL);

	printf("\nImported Table View:\n");
	cdb_printProjection(db, proj, false, true, NULL);

	cdb_freeProjection(proj);

	// Save as a newly generated .cdb to test roundtrip
	ok = cdb_save(db, "inventory_reimported.cdb");
	assert(ok);
	printf("PASS: Saved re-imported table as inventory_reimported.cdb\n");

	cdb_Destroy(db);

	printf("All tests passed cleanly!\n");
	return 0;
}
