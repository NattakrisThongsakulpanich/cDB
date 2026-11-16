#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "./cDB_Table.h"
#include "./cDB.h"

const char MAGIC_BYTE[12] = {0x64, 0x65, 0x31, 0x77, 0x68, 0x6f, 0x61, 0x73, 0x6b, 0x65, 0x64, '\0'}; 

static inline size_t cDB_getDatatypeSize(cDB_Datatype type, size_t custom_size)
{
	switch (type) {
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

// initialize a cDB fsm
void cDB_clearResult(cDB_Database* db)
{
	if (!db) return;
	if (db->qResult.inCondRecords) {
		free(db->qResult.inCondRecords);
		db->qResult.inCondRecords = NULL;
	}
	db->qResult.nResult = 0;
}

cDB_Database* cDB_Init()
{
	cDB_Database* db = (cDB_Database*)malloc(sizeof(cDB_Database));
	if(!db) return NULL;

	db->table = NULL;
	db->qResult.inCondRecords = NULL;
	db->qResult.nResult = 0;
	db->isSaved = true;

	return db;
};

bool cDB_Save(cDB_Database* db, const char filename[65])
{
	if (!db || !db->table) return false;

	const char* target_file = filename;
	char filename_cdb[128];
	if (!target_file || target_file[0] == '\0') {
		if (db->table->name[0] != '\0') {
			snprintf(filename_cdb, sizeof(filename_cdb), "%s.cdb", db->table->name);
			target_file = filename_cdb;
		} else {
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

	bool ok = cDB_Table_fwrite(db->table, f);
	fclose(f);

	if (ok) {
		db->isSaved = true;
	}
	return ok;
}


bool cDB_Open(cDB_Database* db, const char filename[65])
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

	// Allocate new table container
	cDB_Table* t = cDB_Table_new(maxAttr, maxRec, tablename);
	if (!t) {
		fclose(f);
		return false;
	}

	// Rewind to beginning of metadata for cDB_Table_fread
	fseek(f, meta_pos, SEEK_SET);
	if (!cDB_Table_fread(t, f)) {
		cDB_Table_destroy(t);
		fclose(f);
		return false;
	}

	fclose(f);

	// Flush current active table if unsaved and replace it
	if (db->table) {
		if (!db->isSaved && db->table->name[0] != '\0') {
			cDB_Save(db, NULL);
		}
		cDB_Table_destroy(db->table);
		db->table = NULL;
	}

	cDB_clearResult(db);

	db->table = t;
	db->isSaved = true;

	return true;
}

void cDB_Close(cDB_Database* db)
{
	if (!db) return;

	if (db->table) {
		if (!db->isSaved && db->table->name[0] != '\0') {
			cDB_Save(db, NULL);
		}
		cDB_Table_destroy(db->table);
		db->table = NULL;
	}

	cDB_clearResult(db);

	free(db);
}

bool cDB_newTable(cDB_Database* db, const char name[STD_CDB_STRLEN], const uint32_t maxRecord, const uint32_t maxAttribute)
{
	if(!db || !name || maxRecord == 0 || maxAttribute == 0) return false;

	// Flush current active table if unsaved and has a valid name
	if (db->table)
	{
		if (!db->isSaved && db->table->name[0] != '\0')
		{
			cDB_Save(db, NULL);
		}
		cDB_Table_destroy(db->table);
		db->table = NULL;
	}

	cDB_clearResult(db);

	cDB_Table* t = cDB_Table_new(maxAttribute, maxRecord, name);
	if(!t) return false;

	db->table = t;
	db->isSaved = true;

	return true;	
};

// Attribute Promise helpers and resolution
cDB_Attribute cDB_createAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN], cDB_Datatype type, size_t custom_size)
{
	cDB_Attribute promise = {0};
	promise.isValid = false;
	promise.idx = UINT64_MAX;

	if (!db || !db->table || !alias || alias[0] == '\0') return promise;

	// Check if attribute already exists
	uint64_t existing_idx = cDB_Table_getAttributeIdx(db->table, alias);
	uint64_t target_idx = existing_idx;

	if (target_idx == UINT64_MAX) {
		// Find first free attribute slot
		for (uint64_t i = 0; i < db->table->maxAttribute; i++) {
			if (db->table->AttributeAlias[i][0] == '\0') {
				target_idx = i;
				break;
			}
		}
	}

	if (target_idx == UINT64_MAX || target_idx >= db->table->maxAttribute) {
		return promise; // Table attribute capacity full
	}

	size_t size = cDB_getDatatypeSize(type, custom_size);
	if (size == 0) return promise;

	cDB_Table_setAttributeAlias(db->table, target_idx, alias);
	cDB_Table_setAttributeSize(db->table, target_idx, size);

	strncpy(promise.name, alias, sizeof(promise.name) - 1);
	promise.name[sizeof(promise.name) - 1] = '\0';
	promise.idx = target_idx;
	promise.size = size;
	promise.type = type;
	promise.isValid = true;

	db->isSaved = false;
	return promise;
}

cDB_Attribute cDB_getAttribute(cDB_Database* db, const char alias[STD_CDB_STRLEN])
{
	cDB_Attribute promise = {0};
	promise.isValid = false;
	promise.idx = UINT64_MAX;

	if (!db || !db->table || !alias || alias[0] == '\0') return promise;

	uint64_t idx = cDB_Table_getAttributeIdx(db->table, alias);
	if (idx == UINT64_MAX || idx >= db->table->maxAttribute) {
		return promise;
	}

	strncpy(promise.name, alias, sizeof(promise.name) - 1);
	promise.name[sizeof(promise.name) - 1] = '\0';
	promise.idx = idx;
	promise.size = db->table->AttributeSizes[idx];
	promise.type = CDB_CUSTOM;
	promise.isValid = true;

	return promise;
}

cDB_Record cDB_getRecord(cDB_Database* db, const char alias[STD_CDB_STRLEN])
{
	if(!db || !db->table || !alias) return UINT64_MAX;

	return cDB_Table_getRecordIdx(db->table, alias);
}

void cDB_setAttributeAlias(cDB_Database *db, cDB_Attribute attrib, const char alias[STD_CDB_STRLEN])
{
	if(!db || !db->table || !attrib.isValid) return;

	cDB_Table_setAttributeAlias(db->table, attrib.idx, alias);
	db->isSaved = false;
}

void cDB_setRecordAlias(cDB_Database *db, cDB_Record record, const char alias[STD_CDB_STRLEN])
{
	if(!db || !db->table) return;

	cDB_Table_setRecordAlias(db->table, record, alias);
	db->isSaved = false;
}

void cDB_setRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib, void* data)
{
	if (!db || !db->table || !attrib.isValid || !data) return;

	cDB_Table_setRecord(db->table, record, attrib.idx, data);
	db->isSaved = false;
}

void* cDB_getRecordValue(cDB_Database* db, cDB_Record record, cDB_Attribute attrib)
{
	if (!db || !db->table || !attrib.isValid) return NULL;

	return cDB_Table_getRecord(db->table, record, attrib.idx);
}

bool cDB_select(cDB_Database* db, cDB_Predicate predicate, void* user_data)
{
	if (!db || !db->table) return false;

	cDB_clearResult(db);

	if (db->table->maxRecord == 0) return true;

	cDB_Record* matches = (cDB_Record*)malloc(db->table->maxRecord * sizeof(cDB_Record));
	if (!matches) return false;

	uint64_t count = 0;
	for (uint64_t r = 0; r < db->table->maxRecord; r++) {
		// Skip empty/unassigned record slots
		if (db->table->RecordAlias[r][0] == '\0') continue;

		if (!predicate || predicate(db, r, user_data)) {
			matches[count++] = r;
		}
	}

	if (count > 0) {
		cDB_Record* resized = (cDB_Record*)realloc(matches, count * sizeof(cDB_Record));
		db->qResult.inCondRecords = resized ? resized : matches;
		db->qResult.nResult = count;
	} else {
		free(matches);
		db->qResult.inCondRecords = NULL;
		db->qResult.nResult = 0;
	}

	return true;
}

cDB_Record cDB_resultGetRecord(const cDB_Database* db, uint64_t result_idx)
{
	if (!db || !db->qResult.inCondRecords || result_idx >= db->qResult.nResult) {
		return UINT64_MAX;
	}
	return db->qResult.inCondRecords[result_idx];
}

uint64_t cDB_resultLen(const cDB_Database* db)
{
	if(!db) return UINT64_MAX;
	return db->qResult.nResult;
}

void* cDB_resultGetValue(cDB_Database* db, uint64_t result_idx, cDB_Attribute attrib)
{
	cDB_Record rec = cDB_resultGetRecord(db, result_idx);
	if (rec == UINT64_MAX) return NULL;

	return cDB_getRecordValue(db, rec, attrib);
}

void cDB_freeProjection(cDB_Projection* proj)
{
	if (!proj) return;
	if (proj->projRecords) {
		free(proj->projRecords);
		proj->projRecords = NULL;
	}
	if (proj->projAttributes) {
		free(proj->projAttributes);
		proj->projAttributes = NULL;
	}
	free(proj);
}

cDB_Projection* cDB_getProjection(cDB_Database* db, cDB_Record* records, uint64_t nRecords, const uint64_t* attrib_indices, uint64_t nAttributes)
{
	if (!db || !db->table) return NULL;

	cDB_Projection* proj = (cDB_Projection*)malloc(sizeof(cDB_Projection));
	if (!proj) return NULL;

	proj->nRecords = nRecords;
	proj->nAttributes = nAttributes;
	proj->projRecords = NULL;
	proj->projAttributes = NULL;

	if (nAttributes > 0) {
		proj->projAttributes = (uint64_t*)malloc(nAttributes * sizeof(uint64_t));
		if (!proj->projAttributes) {
			free(proj);
			return NULL;
		}

		if (attrib_indices) {
			memcpy(proj->projAttributes, attrib_indices, nAttributes * sizeof(uint64_t));
		} else {
			// If attrib_indices is NULL, take sequential attribute indices 0..nAttributes-1
			for (uint64_t i = 0; i < nAttributes; i++) {
				proj->projAttributes[i] = i;
			}
		}
	}

	if (nRecords > 0) {
		proj->projRecords = (cDB_Record*)malloc(nRecords * sizeof(cDB_Record));
		if (!proj->projRecords) {
			if (proj->projAttributes) free(proj->projAttributes);
			free(proj);
			return NULL;
		}

		if (records) {
			memcpy(proj->projRecords, records, nRecords * sizeof(cDB_Record));
		} else {
			// If records is NULL, take sequential records 0..nRecords-1
			for (uint64_t i = 0; i < nRecords; i++) {
				proj->projRecords[i] = i;
			}
		}
	}

	return proj;
}

cDB_Projection* cDB_getResultProjection(cDB_Database* db)
{
	if (!db || !db->table) return NULL;

	// Collect all active attributes in the table
	uint64_t active_attrs = 0;
	for (uint64_t i = 0; i < db->table->maxAttribute; i++) {
		if (db->table->AttributeAlias[i][0] != '\0') {
			active_attrs++;
		}
	}

	if (active_attrs == 0) return NULL;

	uint64_t* attr_list = (uint64_t*)malloc(active_attrs * sizeof(uint64_t));
	if (!attr_list) return NULL;

	uint64_t k = 0;
	for (uint64_t i = 0; i < db->table->maxAttribute; i++) {
		if (db->table->AttributeAlias[i][0] != '\0') {
			attr_list[k++] = i;
		}
	}

	cDB_Projection* proj = cDB_getProjection(db, db->qResult.inCondRecords, db->qResult.nResult, attr_list, active_attrs);
	free(attr_list);

	return proj;
}

cDB_Projection* cDB_getTableProjection(cDB_Database* db)
{
	if (!db || !db->table) return NULL;

	// Collect active records
	uint64_t active_recs = 0;
	for (uint64_t r = 0; r < db->table->maxRecord; r++) {
		if (db->table->RecordAlias[r][0] != '\0') {
			active_recs++;
		}
	}

	// Collect active attributes
	uint64_t active_attrs = 0;
	for (uint64_t a = 0; a < db->table->maxAttribute; a++) {
		if (db->table->AttributeAlias[a][0] != '\0') {
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
	for (uint64_t r = 0; r < db->table->maxRecord; r++) {
		if (db->table->RecordAlias[r][0] != '\0') {
			rec_list[r_count++] = r;
		}
	}

	uint64_t a_count = 0;
	for (uint64_t a = 0; a < db->table->maxAttribute; a++) {
		if (db->table->AttributeAlias[a][0] != '\0') {
			attr_list[a_count++] = a;
		}
	}

	cDB_Projection* proj = cDB_getProjection(db, rec_list, active_recs, attr_list, active_attrs);
	free(rec_list);
	free(attr_list);

	return proj;
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

void cDB_printProjection
(
	const cDB_Database* db, 
	const cDB_Projection* proj, 
	bool hSeperator, 
	bool vSeperator, 
	cDB_PrintStyle* printStyle, 
	uint8_t* hPadding, uint8_t* vPadding
)
{
	if (!db || !db->table || !proj) return;

	cDB_PrintStyle style = { '+', '+', '+', '+', '|', '-' };
	if (printStyle) {
		style = *printStyle;
	}

	uint8_t pad_h = hPadding ? *hPadding : 1;
	uint8_t pad_v = vPadding ? *vPadding : 0;

	uint64_t totalCols = 1 + proj->nAttributes;
	uint32_t* col_widths = (uint32_t*)malloc(totalCols * sizeof(uint32_t));
	if (!col_widths) return;

	// Initial widths from headers
	col_widths[0] = (uint32_t)strlen("RECORD");
	for (uint64_t j = 0; j < proj->nAttributes; j++) {
		uint64_t attr_idx = proj->projAttributes[j];
		const char* attr_name = (attr_idx < db->table->maxAttribute) ? db->table->AttributeAlias[attr_idx] : "UNKNOWN";
		col_widths[1 + j] = (uint32_t)strlen(attr_name);
	}

	// Calculate widths from records
	for (uint64_t i = 0; i < proj->nRecords; i++) {
		cDB_Record r = proj->projRecords[i];
		const char* rec_alias = (r < db->table->maxRecord) ? db->table->RecordAlias[r] : "";
		char rec_label[STD_CDB_STRLEN];
		if (rec_alias && rec_alias[0] != '\0') {
			snprintf(rec_label, sizeof(rec_label), "%s (%llu)", rec_alias, (unsigned long long)r);
		} else {
			snprintf(rec_label, sizeof(rec_label), "%llu", (unsigned long long)r);
		}
		uint32_t r_len = (uint32_t)strlen(rec_label);
		if (r_len > col_widths[0]) col_widths[0] = r_len;

		for (uint64_t j = 0; j < proj->nAttributes; j++) {
			uint64_t attr_idx = proj->projAttributes[j];
			void* val = cDB_Table_getRecord(db->table, r, attr_idx);
			uint32_t v_len = val ? (uint32_t)strlen((char*)val) : 4; // "NULL"
			if (v_len > col_widths[1 + j]) col_widths[1 + j] = v_len;
		}
	}

	char v_char = vSeperator ? style.VerticalLine : ' ';
	char mid_h = vSeperator ? '+' : style.HorizontalLine;

	// Top border
	print_horizontal_border(style.UpperLeft_Corner, style.HorizontalLine, mid_h, style.UpperRight_Corner, col_widths, totalCols, pad_h);

	// Column header
	putchar(v_char);
	for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
	printf("%-*s", (int)col_widths[0], "RECORD");
	for (uint8_t p = 0; p < pad_h; p++) putchar(' ');

	for (uint64_t j = 0; j < proj->nAttributes; j++) {
		putchar(v_char);
		uint64_t attr_idx = proj->projAttributes[j];
		const char* attr_name = (attr_idx < db->table->maxAttribute) ? db->table->AttributeAlias[attr_idx] : "UNKNOWN";
		for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
		printf("%-*s", (int)col_widths[1 + j], attr_name);
		for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
	}
	putchar(v_char);
	putchar('\n');

	// Header bottom border
	print_horizontal_border('+', style.HorizontalLine, mid_h, '+', col_widths, totalCols, pad_h);

	// Rows
	for (uint64_t i = 0; i < proj->nRecords; i++) {
		// Vertical top padding
		for (uint8_t vp = 0; vp < pad_v; vp++) {
			putchar(v_char);
			for (uint64_t c = 0; c < totalCols; c++) {
				for (uint32_t s = 0; s < col_widths[c] + (pad_h * 2); s++) putchar(' ');
				if (c + 1 < totalCols) putchar(v_char);
			}
			putchar(v_char);
			putchar('\n');
		}

		cDB_Record r = proj->projRecords[i];
		const char* rec_alias = (r < db->table->maxRecord) ? db->table->RecordAlias[r] : "";
		char rec_label[STD_CDB_STRLEN];
		if (rec_alias && rec_alias[0] != '\0') {
			snprintf(rec_label, sizeof(rec_label), "%s (%llu)", rec_alias, (unsigned long long)r);
		} else {
			snprintf(rec_label, sizeof(rec_label), "%llu", (unsigned long long)r);
		}

		putchar(v_char);
		for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
		printf("%-*s", (int)col_widths[0], rec_label);
		for (uint8_t p = 0; p < pad_h; p++) putchar(' ');

		for (uint64_t j = 0; j < proj->nAttributes; j++) {
			putchar(v_char);
			uint64_t attr_idx = proj->projAttributes[j];
			void* val = cDB_Table_getRecord(db->table, r, attr_idx);
			for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
			if (!val) {
				printf("%-*s", (int)col_widths[1 + j], "NULL");
			} else {
				printf("%-*s", (int)col_widths[1 + j], (char*)val);
			}
			for (uint8_t p = 0; p < pad_h; p++) putchar(' ');
		}
		putchar(v_char);
		putchar('\n');

		// Vertical bottom padding
		for (uint8_t vp = 0; vp < pad_v; vp++) {
			putchar(v_char);
			for (uint64_t c = 0; c < totalCols; c++) {
				for (uint32_t s = 0; s < col_widths[c] + (pad_h * 2); s++) putchar(' ');
				if (c + 1 < totalCols) putchar(v_char);
			}
			putchar(v_char);
			putchar('\n');
		}

		// Optional row separator
		if (hSeperator && (i + 1 < proj->nRecords)) {
			print_horizontal_border(mid_h, style.HorizontalLine, mid_h, mid_h, col_widths, totalCols, pad_h);
		}
	}

	// Bottom border
	print_horizontal_border(style.LowerLeft_Corner, style.HorizontalLine, mid_h, style.LowerRight_Corner, col_widths, totalCols, pad_h);
	putchar('\n');

	free(col_widths);
}
