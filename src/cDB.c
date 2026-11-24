#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cDB_Table.h"
#include "../include/cDB.h"

const char MAGIC_BYTE[12] = {0x64, 0x65, 0x31, 0x77, 0x68, 0x6f, 0x61, 0x73, 0x6b, 0x65, 0x64, '\0'}; 

//-----------------------------------Helper Functions---------------------------------------- 

static inline size_t cdb_get_datatype_size(cDB_Datatype type, size_t custom_size)
{
	switch (type) {
		case CDB_BOOL:	  return sizeof(bool);
		case CDB_INT32:   return sizeof(int32_t);
		case CDB_INT64:   return sizeof(int64_t);
		case CDB_UINT32:  return sizeof(uint32_t);
		case CDB_UINT64:  return sizeof(uint64_t);
		case CDB_FLOAT32: return sizeof(float);
		case CDB_FLOAT64: return sizeof(double);
		case CDB_STRING:  return custom_size > 0 ? custom_size : 64;
		case CDB_CUSTOM:  return custom_size;
		default:          return custom_size > 0 ? custom_size : sizeof(uint64_t);
	}
}

//**DEPRECATED**
// * this just guess the type and its make falsely printed format
__declspec(deprecated("DON'T USE THIS FOR CONVERSION")) static cDB_Datatype infer_attr_type(size_t size)
{
	switch (size) {
		case 1:  return CDB_BOOL;
		case 4:  return CDB_INT32;
		case 8:  return CDB_INT64;
		default: return (size > 0) ? CDB_STRING : CDB_CUSTOM;
	}
}

// Helper to write a field adhering to RFC 4180
static void write_rfc4180_field(FILE* f, const char* str)
{
	if (!str) {
		return;
	}

	bool needs_quotes = false;
	const char* p = str;
	while (*p) {
		if (*p == ',' || *p == '"' || *p == '\r' || *p == '\n') {
			needs_quotes = true;
			break;
		}
		p++;
	}

	if (!needs_quotes) {
		fputs(str, f);
		return;
	}

	// RFC 4180: Enclose in double quotes, and escape internal quotes by doubling them ("")
	fputc('"', f);
	p = str;
	while (*p) {
		if (*p == '"') {
			fputc('"', f);
			fputc('"', f);
		} else {
			fputc(*p, f);
		}
		p++;
	}
	fputc('"', f);
}

// Helper to read a logical RFC 4180 line, accounting for quotes that span newlines
static bool read_csv_line(FILE* f, char* line_buf, size_t max_size)
{
	if (!f || !line_buf || max_size == 0) return false;

	size_t idx = 0;
	bool in_quotes = false;
	int c;

	while ((c = fgetc(f)) != EOF) {
		if (c == '"') {
			in_quotes = !in_quotes;
		}

		if (!in_quotes && c == '\n') {
			break; // end of logical line
		}

		if (idx + 1 < max_size) {
			line_buf[idx++] = (char)c;
		}
	}

	if (c == EOF && idx == 0) {
		return false; // end of file
	}

	line_buf[idx] = '\0';
	return true;
}

// Helper to parse the next field from an RFC 4180 line
static bool parse_csv_field(const char** cursor, char* field_buf, size_t max_size)
{
	if (!cursor || !*cursor || !field_buf || max_size == 0) return false;

	const char* p = *cursor;
	if (*p == '\0') return false;

	size_t out_idx = 0;
	if (*p == '"') {
		p++; // skip opening quote
		while (*p) {
			if (*p == '"') {
				if (*(p + 1) == '"') {
					// Escaped quote
					if (out_idx + 1 < max_size) field_buf[out_idx++] = '"';
					p += 2;
				} else {
					// Closing quote
					p++;
					break;
				}
			} else {
				if (out_idx + 1 < max_size) field_buf[out_idx++] = *p;
				p++;
			}
		}
		// Skip up to delimiter or end
		while (*p && *p != ',') p++;
		if (*p == ',') p++;
	} else {
		while (*p && *p != ',' && *p != '\r' && *p != '\n') {
			if (out_idx + 1 < max_size) field_buf[out_idx++] = *p;
			p++;
		}
		if (*p == ',') {
			p++;
		} else {
			// strip trailing \r or \n
			while (*p == '\r' || *p == '\n') p++;
		}
	}

	field_buf[out_idx] = '\0';
	*cursor = p;
	return true;
}

// Helper to compare two records by an attribute's value
static int compare_records(cDB_Database* db, cDB_Record r1, cDB_Record r2, cDB_Attribute* by)
{
	if (!db || !by || !by->isValid) return 0;

	void* p1 = cdb_get_t(db, r1, by);
	void* p2 = cdb_get_t(db, r2, by);

	// Handle NULL values: NULL is considered smaller than non-NULL
	if (!p1 && !p2) return 0;
	if (!p1) return -1;
	if (!p2) return 1;

	switch (by->type) {
		case CDB_BOOL: {
			bool b1 = *(bool*)p1;
			bool b2 = *(bool*)p2;
			return (int)b1 - (int)b2;
		}
		case CDB_INT32: {
			int32_t v1 = *(int32_t*)p1;
			int32_t v2 = *(int32_t*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_INT64: {
			int64_t v1 = *(int64_t*)p1;
			int64_t v2 = *(int64_t*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_UINT32: {
			uint32_t v1 = *(uint32_t*)p1;
			uint32_t v2 = *(uint32_t*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_UINT64: {
			uint64_t v1 = *(uint64_t*)p1;
			uint64_t v2 = *(uint64_t*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_FLOAT32: {
			float v1 = *(float*)p1;
			float v2 = *(float*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_FLOAT64: {
			double v1 = *(double*)p1;
			double v2 = *(double*)p2;
			return (v1 > v2) - (v1 < v2);
		}
		case CDB_STRING: {
			return strcmp((const char*)p1, (const char*)p2);
		}
		case CDB_CUSTOM:
		default: {
			size_t sz = by->size ? by->size : sizeof(uint64_t);
			return memcmp(p1, p2, sz);
		}
	}
}

//-----------------------------------Database lifecycle----------------------------------------

cDB_Database* cdb_Create()
{
	cDB_Database* db = (cDB_Database*)malloc(sizeof(cDB_Database));
	if(!db) return NULL;

	db->table = NULL;
	db->result.matchingRecords = NULL;
	db->result.resultCount = 0;
	db->isSaved = true;
	db->type_ref = NULL;

	return db;
};

void cdb_Destroy(cDB_Database* db)
{
	if (!db) return;

	if (db->table) {
		if (!db->isSaved && db->table->name[0] != '\0') {
			cdb_save(db, NULL);
		}
		cdb_table_Destroy(db->table);
		db->table = NULL;
	}

	if(db->type_ref)
	{
		free(db->type_ref);
		db->type_ref = NULL;
	}

	cdb_clearResult(db);

	free(db);
}

//-----------------------------------File/IO----------------------------------------

bool cdb_save(cDB_Database* db, const char filename[65])
{
	if (!db || !db->table) return false;

	const char* target_file = filename;
	char filename_cdb[128];
	if (!target_file || target_file[0] == '\0') 
	{
		if (db->table->name[0] != '\0') 
		{
			snprintf(filename_cdb, sizeof(filename_cdb), "%s.cdb", db->table->name);
			target_file = filename_cdb;
		} 
		else 
		{
			return false;
		}
	}

	FILE* f = fopen(target_file, "wb");
	if (!f) return false;

	// Write magic byte header
	if (fwrite(MAGIC_BYTE, sizeof(char), sizeof(MAGIC_BYTE), f) != sizeof(MAGIC_BYTE)) {
		fclose(f);
		return false;
	}

	// table write
	bool ok = cdb_table_writeFile(db->table, f);

	//type reference
	fwrite(db->type_ref, sizeof(cDB_Datatype), db->table->attributeCapacity, f);

	fclose(f);

	if (ok) {
		db->isSaved = true;
	}
	return ok;
}

bool cdb_open(cDB_Database* db, const char filename[65])
{
	if (!db || !filename || filename[0] == '\0') return false;

	char filename_buf[128];
	const char* target_file = filename;

	FILE* f = fopen(target_file, "rb");
	if (!f) {
		// Try appending .cdb if not found directly
		snprintf(filename_buf, sizeof(filename_buf), "%s.cdb", filename);
		target_file = filename_buf;
		f = fopen(target_file, "rb");
		if (!f) return false;
	}

	// Validate magic bytes
	char magic[sizeof(MAGIC_BYTE)];
	if (fread(magic, sizeof(char), sizeof(MAGIC_BYTE), f) != sizeof(MAGIC_BYTE) ||
	    memcmp(magic, MAGIC_BYTE, sizeof(MAGIC_BYTE)) != 0) {
		fclose(f);
		return false;
	}

	//**Deprecated**
	// * non linear reading
	/*
	// Read table bounds from file stream to allocate table properly
	long meta_pos = ftell(f);
	uint32_t maxAttr = 0;
	uint32_t maxRec = 0;
	if (fread(&maxAttr, sizeof(uint32_t), 1, f) != 1 ||
	    fread(&maxRec, sizeof(uint32_t), 1, f) != 1) {
		fclose(f);
		return false;
	}

	// Extract table name from filename
	char tablename[65] = {0};
	const char* base = strrchr(filename, '/');
	if (!base) base = strrchr(filename, '\\');
	base = (base) ? base + 1 : filename;
	strncpy(tablename, base, 64);
	tablename[64] = '\0';
	char* dot = strrchr(tablename, '.');
	if (dot && strcmp(dot, ".cdb") == 0) {
		*dot = '\0';
	}
	*/

	// Allocate new table container
	cDB_Table* t = cdb_table_readFile(f);
	if (!t) {
		fclose(f);
		return false;
	}

	//new typerefs
	cDB_Datatype* type_ref = (cDB_Datatype*)calloc(t->attributeCapacity, sizeof(cDB_Datatype));
	if(!type_ref)
	{
		cdb_table_Destroy(t);
		fclose(f);
		return false;	
	}
	fread(type_ref, sizeof(cDB_Datatype), t->attributeCapacity, f);

	fclose(f);

	// Flush current active table if unsaved and replace it
	if (db->table) {
		if (!db->isSaved && db->table->name[0] != '\0') {
			cdb_save(db, NULL);
		}
		cdb_table_Destroy(db->table);
		db->table = NULL;
		if(db->type_ref)
		{
			free(db->type_ref);
			db->type_ref = NULL;
		}
	}

	cdb_clearResult(db);

	db->table = t;
	db->isSaved = true;
	db->type_ref = type_ref;

	return true;
}

// NOTE** : RFC 4180 compliant CSV export
bool cdb_exportCSV(cDB_Database *db, const cDB_Projection *proj, const char *filename)
{
	if (!db || !db->table || !proj || !filename || filename[0] == '\0') {
		return false;
	}

	FILE* fptr = fopen(filename, "wb");
	if (!fptr) {
		return false;
	}

	// 1. Write Header row: RECORD, ATTRIB_0, ATTRIB_1, ...
	write_rfc4180_field(fptr, "RECORD");
	for (uint64_t j = 0; j < proj->attributeCount; j++) {
		fputc(',', fptr);
		uint64_t attr_idx = proj->projectedAttributes[j];
		const char* attr_name = (attr_idx < db->table->attributeCapacity) ? db->table->attributeNames[attr_idx] : "";
		write_rfc4180_field(fptr, attr_name);
	}
	// RFC 4180 specifies CRLF as line delimiter
	fputs("\r\n", fptr);

	// 2. Write Data rows
	for (uint64_t i = 0; i < proj->recordCount; i++) {
		cDB_Record r = proj->projectedRecords[i];
		const char* rec_alias = (r < db->table->recordCapacity) ? db->table->recordNames[r] : "";
		char rec_label[CDB_STD_STRLEN];
		if (rec_alias && rec_alias[0] != '\0') {
			snprintf(rec_label, sizeof(rec_label), "%s", rec_alias);
		} else {
			snprintf(rec_label, sizeof(rec_label), "%llu", (unsigned long long)r);
		}

		write_rfc4180_field(fptr, rec_label);

		for (uint64_t j = 0; j < proj->attributeCount; j++) {

			fputc(',', fptr);

			uint64_t attr_idx = proj->projectedAttributes[j];
			cDB_Attribute attr = cdb_getAttribute_byIndex(db, attr_idx);
			char formatv[128];

			cdb_format_value(db, r, &attr, formatv);

			if (strlen(formatv) > 0) {
				write_rfc4180_field(fptr, formatv);
			}

		}
		fputs("\r\n", fptr);
	}

	

	fclose(fptr);
	return true;
}

bool cdb_importCSV(cDB_Database *db, const char *filename, cDB_Datatype* format)
{
	if (!db || !filename || filename[0] == '\0' || !format) return false;

	FILE* f = fopen(filename, "r");
	if (!f) return false;

	char line_buf[4096];
	char field_buf[1024];

	// Read header row
	if (!read_csv_line(f, line_buf, sizeof(line_buf))) {
		fclose(f);
		return false;
	}

	// Count number of columns in header
	const char* ptr = line_buf;
	uint32_t total_cols = 0;
	while (parse_csv_field(&ptr, field_buf, sizeof(field_buf))) {
		total_cols++;
	}

	if (total_cols < 2) {
		// Need at least RECORD and 1 attribute column
		fclose(f);
		return false;
	}

	uint32_t attr_count = total_cols - 1;

	// Count records (remaining rows)
	uint32_t record_count = 0;
	while (read_csv_line(f, line_buf, sizeof(line_buf))) {
		// Skip empty lines
		const char* p = line_buf;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (*p != '\0') {
			record_count++;
		}
	}

	if (record_count == 0) {
		fclose(f);
		return false;
	}

	// Derive table name from filename
	char tablename[CDB_STD_STRLEN] = {0};
	const char* base = strrchr(filename, '/');
	if (!base) base = strrchr(filename, '\\');
	base = (base) ? base + 1 : filename;
	strncpy(tablename, base, CDB_STD_STRLEN - 1);
	tablename[CDB_STD_STRLEN - 1] = '\0';
	char* dot = strrchr(tablename, '.');
	if (dot && (strcmp(dot, ".csv") == 0 || strcmp(dot, ".CSV") == 0)) {
		*dot = '\0';
	}

	// Create table
	if (!cdb_CreateTable(db, tablename, record_count, attr_count)) {
		fclose(f);
		return false;
	}

	// Rewind to beginning and re-read header to create attributes
	rewind(f);
	read_csv_line(f, line_buf, sizeof(line_buf));
	ptr = line_buf;

	// Skip RECORD header
	parse_csv_field(&ptr, field_buf, sizeof(field_buf));

	// Create attributes
	for (uint32_t j = 0; j < attr_count; j++) {
		if (!parse_csv_field(&ptr, field_buf, sizeof(field_buf))) {
			snprintf(field_buf, sizeof(field_buf), "ATTR_%u", j);
		}
		cDB_Datatype dt = format[j];
		size_t custom_sz = (dt == CDB_STRING) ? 64 : 0;
		cdb_CreateAttribute(db, field_buf, dt, custom_sz);
	}

	// Read and populate data rows
	uint32_t current_row = 0;
	while (current_row < record_count && read_csv_line(f, line_buf, sizeof(line_buf))) {
		const char* p = line_buf;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (*p == '\0') continue; // skip blank line

		ptr = line_buf;

		// 1. First field is Record Alias / Name
		if (parse_csv_field(&ptr, field_buf, sizeof(field_buf))) {
			cdb_set_recordName(db, current_row, field_buf);
		}

		// 2. Next fields are attributes
		for (uint32_t j = 0; j < attr_count; j++) {
			if (!parse_csv_field(&ptr, field_buf, sizeof(field_buf))) {
				field_buf[0] = '\0';
			}

			cDB_Attribute attr = cdb_getAttribute_byIndex(db, j);
			if (!attr.isValid) continue;

			switch (format[j]) {
				case CDB_BOOL: {
					bool val = (strcmp(field_buf, "true") == 0 || strcmp(field_buf, "1") == 0);
					cdb_set_bool(db, current_row, &attr, val);
					break;
				}
				case CDB_INT32: {
					int32_t val = (int32_t)strtol(field_buf, NULL, 10);
					cdb_set_i32(db, current_row, &attr, val);
					break;
				}
				case CDB_INT64: {
					int64_t val = (int64_t)strtoll(field_buf, NULL, 10);
					cdb_set_i64(db, current_row, &attr, val);
					break;
				}
				case CDB_UINT32: {
					uint32_t val = (uint32_t)strtoul(field_buf, NULL, 10);
					cdb_set_u32(db, current_row, &attr, val);
					break;
				}
				case CDB_UINT64: {
					uint64_t val = (uint64_t)strtoull(field_buf, NULL, 10);
					cdb_set_u64(db, current_row, &attr, val);
					break;
				}
				case CDB_FLOAT32: {
					float val = (float)strtod(field_buf, NULL);
					cdb_set_f32(db, current_row, &attr, val);
					break;
				}
				case CDB_FLOAT64: {
					double val = strtod(field_buf, NULL);
					cdb_set_f64(db, current_row, &attr, val);
					break;
				}
				case CDB_STRING: {
					cdb_set_string(db, current_row, &attr, field_buf);
					break;
				}
				case CDB_CUSTOM:
				default: {
					// Fallback: store as string if buffer fits
					cdb_set_string(db, current_row, &attr, field_buf);
					break;
				}
			}
		}

		current_row++;
	}

	fclose(f);
	db->isSaved = false;
	return true;
}

//-----------------------------------Table manipulation----------------------------------------

bool cdb_CreateTable(cDB_Database* db, const char name[CDB_STD_STRLEN], const uint32_t recordCapacity, const uint32_t attributeCapacity)
{
	if(!db || !name || recordCapacity == 0 || attributeCapacity == 0) return false;

	// Flush current active table if unsaved and has a valid name
	if (db->table)
	{
		if (!db->isSaved && db->table->name[0] != '\0')
		{
			cdb_save(db, NULL);
		}
		cdb_table_Destroy(db->table);
		db->table = NULL;
	}

	cdb_clearResult(db);

	//new Table and typeref
	cDB_Table* t = cdb_table_Create(attributeCapacity, recordCapacity, name);
	if(!t) return false;

	cDB_Datatype* type_ref = (cDB_Datatype*)calloc(attributeCapacity, sizeof(cDB_Datatype)); //this default to customs
	if(!type_ref)
	{
		cdb_table_Destroy(t);
		return false;
	}

	db->table 	= t;
	db->isSaved = true;
	db->type_ref = type_ref;

	return true;	
};

//-----------------------------------Attribute operations----------------------------------------

cDB_Attribute cdb_CreateAttribute(cDB_Database* db, const char alias[CDB_STD_STRLEN], cDB_Datatype type, size_t custom_size)
{
	cDB_Attribute promise = {0};
	promise.isValid = false;
	promise.index = UINT64_MAX;

	if (!db || !db->table || !alias || alias[0] == '\0') return promise;

	// Check if attribute already exists
	uint64_t existing_idx = cdb_table_getAttributeIndex(db->table, alias);
	uint64_t target_idx = existing_idx;

	if (target_idx == UINT64_MAX) {
		// Find first free attribute slot
		for (uint64_t i = 0; i < db->table->attributeCapacity; i++) {
			if (db->table->attributeNames[i][0] == '\0') {
				target_idx = i;
				break;
			}
		}
	}

	if (target_idx == UINT64_MAX || target_idx >= db->table->attributeCapacity) {
		return promise; // Table attribute capacity full
	}

	size_t size = cdb_get_datatype_size(type, custom_size);
	if (size == 0) return promise;

	cdb_table_setAttributeName(db->table, target_idx, alias);
	cdb_table_setAttributeSize(db->table, target_idx, size);

	strncpy(promise.name, alias, sizeof(promise.name) - 1);
	promise.name[sizeof(promise.name) - 1] = '\0';
	promise.index = target_idx;
	promise.size = size;
	promise.type = type;
	promise.isValid = true;

	if(db->type_ref)
	{
		db->type_ref[target_idx] = type;
	}

	db->isSaved = false;
	return promise;
}

cDB_Attribute cdb_getAttribute(cDB_Database* db, const char alias[CDB_STD_STRLEN])
{
	cDB_Attribute promise = {0};
	promise.isValid = false;
	promise.index = UINT64_MAX;

	if (!db || !db->table || !alias || alias[0] == '\0') return promise;

	uint64_t index = cdb_table_getAttributeIndex(db->table, alias);
	if (index == UINT64_MAX || index >= db->table->attributeCapacity) {
		return promise;
	}

	strncpy(promise.name, alias, sizeof(promise.name) - 1);
	promise.name[sizeof(promise.name) - 1] = '\0';
	promise.index = index;
	promise.size = db->table->attributeSizes[index];
	promise.type = db->type_ref[index];
	promise.isValid = true;

	return promise;
}

cDB_Attribute cdb_getAttribute_byIndex(cDB_Database* db, uint64_t attrib_idx)
{
	cDB_Attribute promise = {0};
	promise.isValid = false;
	promise.index = UINT64_MAX;

	if (!db || !db->table || attrib_idx >= db->table->attributeCapacity) return promise;

	cdb_table_copyAttributeName(db->table, attrib_idx, promise.name);
	promise.name[sizeof(promise.name) - 1] = '\0';
	promise.index = attrib_idx;
	promise.size = db->table->attributeSizes[attrib_idx];
	promise.type = db->type_ref[attrib_idx];
	promise.isValid = true;

	return promise;
};

void cdb_setAttributeName(cDB_Database *db, cDB_Attribute* attrib, const char alias[CDB_STD_STRLEN])
{
	if(!db || !db->table || !attrib->isValid) return;

	cdb_table_setAttributeName(db->table, attrib->index, alias);
	db->isSaved = false;
}



//-----------------------------------Record operations----------------------------------------

cDB_Record cdb_get_record(cDB_Database* db, const char alias[CDB_STD_STRLEN])
{
	if(!db || !db->table || !alias) return UINT64_MAX;

	return cdb_table_getRecordIndex(db->table, alias);
}

void cdb_set_recordName(cDB_Database *db, cDB_Record record, const char alias[CDB_STD_STRLEN])
{
	if(!db || !db->table) return;

	cdb_table_setRecordName(db->table, record, alias);
	db->isSaved = false;
}

void cdb_swapRecord(cDB_Database* db, uint64_t record1, uint64_t record2)
{
	if (!db || !db->table) return;
	if (record1 >= db->table->recordCapacity || record2 >= db->table->recordCapacity) return;
	if (record1 == record2) return;

	// 1. Swap Record Names (Aliases)
	char temp_name[65];
	memcpy(temp_name, db->table->recordNames[record1], 65);
	memcpy(db->table->recordNames[record1], db->table->recordNames[record2], 65);
	memcpy(db->table->recordNames[record2], temp_name, 65);

	// 2. Swap all attribute cell values for record1 and record2
	for (uint64_t i = 0; i < db->table->attributeCapacity; i++) {
		if (db->table->data[i] == NULL) continue;

		size_t attr_size = db->table->attributeSizes[i];
		if (attr_size == 0) continue;

		char* col_base = (char*)db->table->data[i];
		char* ptr1 = col_base + (record1 * attr_size);
		char* ptr2 = col_base + (record2 * attr_size);

		// Stack buffer for typical sizes, malloc fallback for larger attributes
		char stack_buf[256];
		void* temp_val = (attr_size <= sizeof(stack_buf)) ? stack_buf : malloc(attr_size);
		if (!temp_val) continue;

		memcpy(temp_val, ptr1, attr_size);
		memcpy(ptr1, ptr2, attr_size);
		memcpy(ptr2, temp_val, attr_size);

		if (temp_val != stack_buf) {
			free(temp_val);
		}
	}

	db->isSaved = false;
}

//-----------------------------------Value access----------------------------------------

// set
void cdb_set_t(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const void* data)
{
	if (!db || !db->table || !attrib->isValid || !data) return;

	cdb_table_set(db->table, record, attrib->index, data);
	db->isSaved = false;
}

// Typed set
void cdb_set_bool(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const bool data)
{
	if(attrib->type != CDB_BOOL) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_i32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const int32_t data)
{
	if(attrib->type != CDB_INT32) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_i64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const int64_t data)
{
	if(attrib->type != CDB_INT64) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_u32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const uint32_t data)
{
	if(attrib->type != CDB_UINT32) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_u64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const uint64_t data)
{
	if(attrib->type != CDB_UINT64) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_f32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const float data)
{
	if(attrib->type != CDB_FLOAT32) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_f64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const double data)
{
	if(attrib->type != CDB_FLOAT64) return;
	cdb_set_t(db, record, attrib, &data );
}

void cdb_set_string(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, const char* data)
{
	if(!db || !db->table || !attrib->isValid || !data) return;
	if(record >= db->table->recordCapacity || attrib->index >= db->table->attributeCapacity) return;
	if(attrib->type != CDB_STRING) return;

	size_t max_size = db->table->attributeSizes[attrib->index];
	if(max_size == 0) return;

	if(db->table->data[attrib->index] == NULL)
	{
		db->table->data[attrib->index] = calloc(db->table->recordCapacity, max_size);
		if(!db->table->data[attrib->index]) return;
	}

	char* dest = (char*)db->table->data[attrib->index] + (record * max_size);
	strncpy(dest, data, max_size - 1);
	dest[max_size - 1] = '\0';
	db->isSaved = false;
}

// get
void* cdb_get_t(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (!db || !db->table || !attrib->isValid) return NULL;

	return cdb_table_get(db->table, record, attrib->index);
}

// Typed get
bool cdb_get_bool(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_BOOL) return 0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(bool*)ptr : 0;
}

int32_t cdb_get_i32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_INT32) return 0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(int32_t*)ptr : 0;
}

int64_t cdb_get_i64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_INT64) return 0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(int64_t*)ptr : 0;
}

uint32_t cdb_get_u32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_UINT32) return 0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(uint32_t*)ptr : 0;
}

uint64_t cdb_get_u64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_UINT64) return 0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(uint64_t*)ptr : 0;
}

float cdb_get_f32(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_FLOAT32) return 0.0f;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(float*)ptr : 0.0f;
}

double cdb_get_f64(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_FLOAT64) return 0.0;
	void* ptr = cdb_get_t(db, record, attrib);
	return ptr ? *(double*)ptr : 0.0;
}

char* cdb_get_string(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib)
{
	if (attrib->type != CDB_STRING) return NULL;
	return (char*)cdb_get_t(db, record, attrib);
}

uint64_t cdb_format_value(cDB_Database* db, cDB_Record record, cDB_Attribute* attrib, char* buffer)
{
	if (!db || !db->table || !attrib || !attrib->isValid || !buffer) return 0;

	void* ptr = cdb_get_t(db, record, attrib);
	if (!ptr) {
		int written = sprintf(buffer, "NULL");
		return written > 0 ? (uint64_t)written : 0;
	}

	int written = 0;
	switch (attrib->type) {
		case CDB_BOOL:
			written = sprintf(buffer, *(bool*)ptr ? "true" : "false");
			break;
		case CDB_INT32:
			written = sprintf(buffer, "%d", *(int32_t*)ptr);
			break;
		case CDB_INT64:
			written = sprintf(buffer, "%lld", (long long)*(int64_t*)ptr);
			break;
		case CDB_UINT32:
			written = sprintf(buffer, "%u", *(uint32_t*)ptr);
			break;
		case CDB_UINT64:
			written = sprintf(buffer, "%llu", (unsigned long long)*(uint64_t*)ptr);
			break;
		case CDB_FLOAT32:
			written = sprintf(buffer, "%.3f", *(float*)ptr);
			break;
		case CDB_FLOAT64:
			written = sprintf(buffer, "%.6f", *(double*)ptr);
			break;
		case CDB_STRING:
			written = sprintf(buffer, "%s", (char*)ptr);
			break;
		case CDB_CUSTOM:
		default: {
			// Format as hex representation or generic byte indicator
			size_t sz = attrib->size;
			if (sz == 0) sz = sizeof(uint64_t);
			if (sz > 8) sz = 8; // preview first 8 bytes
			int offset = 0;
			unsigned char* bytes = (unsigned char*)ptr;
			for (size_t i = 0; i < sz; i++) {
				offset += sprintf(buffer + offset, "%02X", bytes[i]);
			}
			written = offset;
			break;
		}
	}

	return written > 0 ? (uint64_t)written : 0;
}

//-----------------------------------select----------------------------------------

void cdb_clearResult(cDB_Database* db)
{
	if (!db) return;
	if (db->result.matchingRecords) {
		free(db->result.matchingRecords);
		db->result.matchingRecords = NULL;
	}
	db->result.resultCount = 0;
}

bool cdb_select(cDB_Database* db, cDB_Predicate predicate, void* user_data)
{
	if (!db || !db->table) return false;

	cdb_clearResult(db);

	if (db->table->recordCapacity == 0) return true;

	cDB_Record* matches = (cDB_Record*)malloc(db->table->recordCapacity * sizeof(cDB_Record));
	if (!matches) return false;

	uint64_t count = 0;
	for (uint64_t r = 0; r < db->table->recordCapacity; r++) {
		// Skip empty/unassigned record slots
		if (db->table->recordNames[r][0] == '\0') continue;

		if (!predicate || predicate(db, r, user_data)) {
			matches[count++] = r;
		}
	}

	if (count > 0) {
		cDB_Record* resized = (cDB_Record*)realloc(matches, count * sizeof(cDB_Record));
		db->result.matchingRecords = resized ? resized : matches;
		db->result.resultCount = count;
	} else {
		free(matches);
		db->result.matchingRecords = NULL;
		db->result.resultCount = 0;
	}

	return true;
}

bool cdb_copyResult(const cDB_Database* db, cDB_Result* dst)
{
	if (!db || !db->result.matchingRecords || !dst) 
	{
		return false;
	}
	
	memcpy(dst->matchingRecords, db->result.matchingRecords, db->result.resultCount * sizeof(uint64_t));
	dst->resultCount = db->result.resultCount;

	return true;
}

const cDB_Result* cdb_getResult(const cDB_Database* db)
{
	if (!db || !db->result.matchingRecords) 
	{
		return NULL;
	}

	return &(db->result);
}

//-----------------------------------sort------------------------------------------

void cdb_sort_DB(cDB_Database *db, cDB_Attribute* by, cDB_SortDirection direction)
{
	if (!db || !db->table || !by || !by->isValid) return;
	if (db->table->recordCapacity < 2) return;

	uint64_t n = db->table->recordCapacity;
	for (uint64_t i = 0; i < n - 1; i++) {
		bool swapped = false;
		for (uint64_t j = 0; j < n - 1 - i; j++) {
			int cmp = compare_records(db, j, j + 1, by);
			bool should_swap = (direction == CDB_SORTDIR_ASCEND) ? (cmp > 0) : (cmp < 0);
			if (should_swap) {
				cdb_swapRecord(db, j, j + 1);
				swapped = true;
			}
		}
		if (!swapped) break;
	}
}

void cdb_sort_Projection(cDB_Database* db, cDB_Projection* projection, cDB_Attribute* by, cDB_SortDirection direction)
{
	if (!db || !db->table || !projection || !projection->projectedRecords || !by || !by->isValid) return;
	if (projection->recordCount < 2) return;

	uint64_t n = projection->recordCount;
	for (uint64_t i = 0; i < n - 1; i++) {
		bool swapped = false;
		for (uint64_t j = 0; j < n - 1 - i; j++) {
			cDB_Record r1 = projection->projectedRecords[j];
			cDB_Record r2 = projection->projectedRecords[j + 1];

			int cmp = compare_records(db, r1, r2, by);
			bool should_swap = (direction == CDB_SORTDIR_ASCEND) ? (cmp > 0) : (cmp < 0);
			if (should_swap) {
				projection->projectedRecords[j] = r2;
				projection->projectedRecords[j + 1] = r1;
				swapped = true;
			}
		}
		if (!swapped) break;
	}
}

//-----------------------------------projections----------------------------------------

cDB_Projection* cdb_CreateProjection(cDB_Database* db, cDB_Record* records, uint64_t recordCount, const uint64_t* attrib_indices, uint64_t attributeCount)
{
	if (!db || !db->table) return NULL;

	cDB_Projection* proj = (cDB_Projection*)malloc(sizeof(cDB_Projection));
	if (!proj) return NULL;

	proj->recordCount = recordCount;
	proj->attributeCount = attributeCount;
	proj->projectedRecords = NULL;
	proj->projectedAttributes = NULL;

	if (attributeCount > 0) {
		proj->projectedAttributes = (uint64_t*)malloc(attributeCount * sizeof(uint64_t));
		if (!proj->projectedAttributes) {
			free(proj);
			return NULL;
		}

		if (attrib_indices) {
			memcpy(proj->projectedAttributes, attrib_indices, attributeCount * sizeof(uint64_t));
		} else {
			// If attrib_indices is NULL, take sequential attribute indices 0..attributeCount-1
			for (uint64_t i = 0; i < attributeCount; i++) {
				proj->projectedAttributes[i] = i;
			}
		}
	}

	if (recordCount > 0) {
		proj->projectedRecords = (cDB_Record*)malloc(recordCount * sizeof(cDB_Record));
		if (!proj->projectedRecords) {
			if (proj->projectedAttributes) free(proj->projectedAttributes);
			free(proj);
			return NULL;
		}

		if (records) {
			memcpy(proj->projectedRecords, records, recordCount * sizeof(cDB_Record));
		} else {
			// If records is NULL, take sequential records 0..recordCount-1
			for (uint64_t i = 0; i < recordCount; i++) {
				proj->projectedRecords[i] = i;
			}
		}
	}

	return proj;
}

cDB_Projection* cdb_CreateResultProjection(cDB_Database* db)
{
	if (!db || !db->table) return NULL;

	// Collect all active attributes in the table
	uint64_t active_attrs = 0;
	for (uint64_t i = 0; i < db->table->attributeCapacity; i++) {
		if (db->table->attributeNames[i][0] != '\0') {
			active_attrs++;
		}
	}

	if (active_attrs == 0) return NULL;

	uint64_t* attr_list = (uint64_t*)malloc(active_attrs * sizeof(uint64_t));
	if (!attr_list) return NULL;

	uint64_t k = 0;
	for (uint64_t i = 0; i < db->table->attributeCapacity; i++) {
		if (db->table->attributeNames[i][0] != '\0') {
			attr_list[k++] = i;
		}
	}

	cDB_Projection* proj = cdb_CreateProjection(db, db->result.matchingRecords, db->result.resultCount, attr_list, active_attrs);
	free(attr_list);

	return proj;
}

cDB_Projection* cdb_CreateTableProjection(cDB_Database* db)
{
	if (!db || !db->table) return NULL;

	// Collect active records
	uint64_t active_recs = 0;
	for (uint64_t r = 0; r < db->table->recordCapacity; r++) {
		if (db->table->recordNames[r][0] != '\0') {
			active_recs++;
		}
	}

	// Collect active attributes
	uint64_t active_attrs = 0;
	for (uint64_t a = 0; a < db->table->attributeCapacity; a++) {
		if (db->table->attributeNames[a][0] != '\0') {
			active_attrs++;
		}
	}

	if (active_attrs == 0 || active_recs == 0) return NULL;

	cDB_Record* rec_list = (cDB_Record*)malloc(active_recs * sizeof(cDB_Record));
	uint64_t* attr_list = (uint64_t*)malloc(active_attrs * sizeof(uint64_t));
	if (!rec_list || !attr_list) {
		if (rec_list) free(rec_list);
		if (attr_list) free(attr_list);
		return NULL;
	}

	uint64_t r_count = 0;
	for (uint64_t r = 0; r < db->table->recordCapacity; r++) {
		if (db->table->recordNames[r][0] != '\0') {
			rec_list[r_count++] = r;
		}
	}

	uint64_t a_count = 0;
	for (uint64_t a = 0; a < db->table->attributeCapacity; a++) {
		if (db->table->attributeNames[a][0] != '\0') {
			attr_list[a_count++] = a;
		}
	}

	cDB_Projection* proj = cdb_CreateProjection(db, rec_list, active_recs, attr_list, active_attrs);
	free(rec_list);
	free(attr_list);

	return proj;
}

void cdb_freeProjection(cDB_Projection* proj)
{
	if (!proj) return;
	if (proj->projectedRecords) {
		free(proj->projectedRecords);
		proj->projectedRecords = NULL;
	}
	if (proj->projectedAttributes) {
		free(proj->projectedAttributes);
		proj->projectedAttributes = NULL;
	}
	free(proj);
}

static void print_horizontal_border
(
	const char left_corner, 
	const char fill_line,
	const char mid_joint, 
	const char right_corner, 
	const uint32_t* col_widths, 
	uint64_t nCols, uint8_t pad_h
)
{
	putchar(left_corner);
	for (uint64_t c = 0; c < nCols; c++) {
		uint32_t total_w = col_widths[c] + (pad_h * 2);
		for (uint32_t i = 0; i < total_w; i++) {
			putchar(fill_line);
		}
		if (c + 1 < nCols) {
			putchar(mid_joint);
		}
	}
	putchar(right_corner);
	putchar('\n');
}

void cdb_printProjection
(
	cDB_Database* db, 
	cDB_Projection* proj, 
	bool hSeperator, 
	bool vSeperator, 
	cDB_PrintStyle* printStyle
)
{
	if (!db || !db->table || !proj) return;

	cDB_PrintStyle style = { '+', '+', '+', '+', '|', '-' };
	if (printStyle) {
		style = *printStyle;
	}

	uint64_t totalCols = 1 + proj->attributeCount;
	uint32_t* col_widths = (uint32_t*)malloc(totalCols * sizeof(uint32_t));
	if (!col_widths) return;

	// Pass 1: Initialize column widths with header lengths
	col_widths[0] = (uint32_t)strlen("RECORD");
	for (uint64_t j = 0; j < proj->attributeCount; j++) {
		uint64_t attr_idx = proj->projectedAttributes[j];
		const char* attr_name = (attr_idx < db->table->attributeCapacity) ? db->table->attributeNames[attr_idx] : "UNKNOWN";
		col_widths[1 + j] = (uint32_t)strlen(attr_name);
	}

	// Pass 1: Find longest formatted value per column
	char buf[256];
	for (uint64_t i = 0; i < proj->recordCount; i++) {
		cDB_Record r = proj->projectedRecords[i];
		const char* rec_alias = (r < db->table->recordCapacity) ? db->table->recordNames[r] : "";
		char rec_label[CDB_STD_STRLEN];
		if (rec_alias && rec_alias[0] != '\0') {
			snprintf(rec_label, sizeof(rec_label), "%s (%llu)", rec_alias, (unsigned long long)r);
		} else {
			snprintf(rec_label, sizeof(rec_label), "%llu", (unsigned long long)r);
		}
		uint32_t r_len = (uint32_t)strlen(rec_label);
		if (r_len > col_widths[0]) col_widths[0] = r_len;

		for (uint64_t j = 0; j < proj->attributeCount; j++) {
			uint64_t attr_idx = proj->projectedAttributes[j];
			if (attr_idx >= db->table->attributeCapacity) continue;

			/*
			cDB_Attribute attr = {0};
			attr.index = attr_idx;
			attr.size = db->table->attributeSizes[attr_idx];
			attr.type = infer_attr_type(attr.size);
			strncpy(attr.name, db->table->attributeNames[attr_idx], CDB_STD_STRLEN - 1);
			attr.isValid = true;
			*/

			cDB_Attribute attr = cdb_getAttribute_byIndex(db, attr_idx);

			uint64_t len = cdb_format_value((cDB_Database*)db, r, &attr, buf);
			if (len > col_widths[1 + j]) {
				col_widths[1 + j] = (uint32_t)len;
			}
		}
	}

	// Apply hybrid upper cap
	for (uint64_t c = 0; c < totalCols; c++) {
		if (col_widths[c] > CDB_PRINT_MAX_COL_WIDTH) {
			col_widths[c] = CDB_PRINT_MAX_COL_WIDTH;
		}
	}

	char v_char = vSeperator ? style.VerticalLine : ' ';
	char mid_h = vSeperator ? '+' : style.HorizontalLine;
	uint8_t pad_h = 1;

	// Top border
	print_horizontal_border(style.UpperLeft_Corner, style.HorizontalLine, mid_h, style.UpperRight_Corner, col_widths, totalCols, pad_h);

	// Column header
	putchar(v_char);
	putchar(' ');
	printf("%-*.*s", (int)col_widths[0], (int)col_widths[0], "RECORD");
	putchar(' ');

	for (uint64_t j = 0; j < proj->attributeCount; j++) {
		putchar(v_char);
		uint64_t attr_idx = proj->projectedAttributes[j];
		const char* attr_name = (attr_idx < db->table->attributeCapacity) ? db->table->attributeNames[attr_idx] : "UNKNOWN";
		putchar(' ');
		printf("%-*.*s", (int)col_widths[1 + j], (int)col_widths[1 + j], attr_name);
		putchar(' ');
	}
	putchar(v_char);
	putchar('\n');

	// Header bottom border
	print_horizontal_border('+', style.HorizontalLine, mid_h, '+', col_widths, totalCols, pad_h);

	// Pass 2: Print rows
	for (uint64_t i = 0; i < proj->recordCount; i++) {
		cDB_Record r = proj->projectedRecords[i];
		const char* rec_alias = (r < db->table->recordCapacity) ? db->table->recordNames[r] : "";
		char rec_label[CDB_STD_STRLEN];
		if (rec_alias && rec_alias[0] != '\0') {
			snprintf(rec_label, sizeof(rec_label), "%s (%llu)", rec_alias, (unsigned long long)r);
		} else {
			snprintf(rec_label, sizeof(rec_label), "%llu", (unsigned long long)r);
		}

		putchar(v_char);
		putchar(' ');
		printf("%-*.*s", (int)col_widths[0], (int)col_widths[0], rec_label);
		putchar(' ');

		for (uint64_t j = 0; j < proj->attributeCount; j++) {
			putchar(v_char);
			uint64_t attr_idx = proj->projectedAttributes[j];

			/*
			cDB_Attribute attr = {0};
			if (attr_idx < db->table->attributeCapacity) {
				attr.index = attr_idx;
				attr.size = db->table->attributeSizes[attr_idx];
				attr.type = infer_attr_type(attr.size);
				attr.isValid = true;
				strncpy(attr.name, db->table->attributeNames[attr_idx], CDB_STD_STRLEN - 1);
			}
			*/

			cDB_Attribute attr = cdb_getAttribute_byIndex(db, attr_idx);

			cdb_format_value((cDB_Database*)db, r, &attr, buf);

			putchar(' ');
			printf("%-*.*s", (int)col_widths[1 + j], (int)col_widths[1 + j], buf);
			putchar(' ');
		}
		putchar(v_char);
		putchar('\n');

		// Optional row separator
		if (hSeperator && (i + 1 < proj->recordCount)) {
			print_horizontal_border(mid_h, style.HorizontalLine, mid_h, mid_h, col_widths, totalCols, pad_h);
		}
	}

	// Bottom border
	print_horizontal_border(style.LowerLeft_Corner, style.HorizontalLine, mid_h, style.LowerRight_Corner, col_widths, totalCols, pad_h);
	putchar('\n');

	free(col_widths);
}