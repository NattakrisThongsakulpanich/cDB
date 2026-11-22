#include <stdio.h>
#include <stdbool.h>
#include "../include/cDB.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename.cdb>\n", argv[0]);
		return 1;
	}

	const char* filename = argv[1];

	cDB_Database* db = cDB_Init();
	if (!db) {
		fprintf(stderr, "Error: Failed to initialize database.\n");
		return 1;
	}

	if (!cDB_Open(db, filename)) {
		fprintf(stderr, "Error: Could not open or invalid .cdb file: %s\n", filename);
		cDB_Close(db);
		return 1;
	}

	cDB_Projection* proj = cDB_getTProjection(db);
	if (!proj) {
		printf("Table is empty or has no active records/attributes.\n");
	} else {
		cDB_printProjection(db, proj, false, true, NULL);
		cDB_freeProjection(proj);
	}

	cDB_Close(db);
	return 0;
}