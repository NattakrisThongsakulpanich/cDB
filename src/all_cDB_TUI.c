#include <stdio.h>
#include <stdbool.h>
#include "../include/cDB.h"


int viewMode(const char* filename)
{
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


int exportCSVMode(const char* filename, const char* output_csv)
{
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
		fprintf(stderr, "Error: Table is empty or has no active records/attributes.\n");
		cDB_Close(db);
		return 1;
	}

	char csv_name_buf[128];
	const char* out_filename = output_csv;
	if (!out_filename || out_filename[0] == '\0') {
		snprintf(csv_name_buf, sizeof(csv_name_buf), "%s.csv", db->table->name[0] ? db->table->name : "export");
		out_filename = csv_name_buf;
	}

	if (!cDB_Proj2CSV(db, proj, out_filename)) {
		fprintf(stderr, "Error: Failed to export CSV to %s\n", out_filename);
		cDB_freeProjection(proj);
		cDB_Close(db);
		return 1;
	}

	printf("Successfully exported table to %s\n", out_filename);
	cDB_freeProjection(proj);
	cDB_Close(db);
	return 0;
}


int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename.cdb> or %s <--view-t|--export-csv> <filename.cdb> [output.csv]\n", argv[0], argv[0]);
		return 1;
	}

	// Direct view mode: cdb <filename.cdb>
	if (argc == 2 && argv[1][0] != '-') {
		return viewMode(argv[1]);
	}

	const char* config = argv[1];
	const char* filename = (argc > 2) ? argv[2] : NULL;

	if (!filename) {
		fprintf(stderr, "Error: Missing input filename for %s\n", config);
		return 1;
	}

	if(strcmp(config, "--view-t") == 0)
	{
		return viewMode(filename);
	}
	else if(strcmp(config, "--export-csv") == 0) 
	{
		const char* output_csv = (argc > 3) ? argv[3] : NULL;
		return exportCSVMode(filename, output_csv);
	}
	else
	{
		fprintf(stderr, "Unknown option: %s\n", config);
		return 1;
	}
}