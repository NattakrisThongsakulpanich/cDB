#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include "../include/cDB.h"


int viewMode(const char* filename)
{
    cDB_Database* db = cdb_Create();
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database.\n");
        return 1;
    }

    if (!cdb_open(db, filename)) {
        fprintf(stderr, "Error: Could not open or invalid .cdb file: %s\n", filename);
        cdb_Destroy(db);
        return 1;
    }

    cDB_Projection* proj = cdb_CreateTableProjection(db);
    if (!proj) {
        printf("Table is empty or has no active records/attributes.\n");
    } else {
        cdb_printProjection(db, proj, false, true, NULL);
        cdb_freeProjection(proj);
    }

    cdb_Destroy(db);
    return 0;
}


int exportCSVMode(const char* filename, const char* output_csv)
{
    cDB_Database* db = cdb_Create();
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database.\n");
        return 1;
    }

    if (!cdb_open(db, filename)) {
        fprintf(stderr, "Error: Could not open or invalid .cdb file: %s\n", filename);
        cdb_Destroy(db);
        return 1;
    }

    cDB_Projection* proj = cdb_CreateTableProjection(db);
    if (!proj) {
        fprintf(stderr, "Error: Table is empty or has no active records/attributes.\n");
        cdb_Destroy(db);
        return 1;
    }

    char csv_name_buf[128];
    const char* out_filename = output_csv;
    if (!out_filename || out_filename[0] == '\0') {
        snprintf(csv_name_buf, sizeof(csv_name_buf), "%s.csv", db->table->name[0] ? db->table->name : "export");
        out_filename = csv_name_buf;
    }

    if (!cdb_exportCSV(db, proj, out_filename)) {
        fprintf(stderr, "Error: Failed to export CSV to %s\n", out_filename);
        cdb_freeProjection(proj);
        cdb_Destroy(db);
        return 1;
    }

    printf("Successfully exported table to %s\n", out_filename);
    cdb_freeProjection(proj);
    cdb_Destroy(db);
    return 0;
}

/* ---------- CSV -> CDB ---------- */

#define CSV_LINE_MAX 4096     /* same limit as cdb_importCSV's line buffer */
#define CSV_STR_MAX 63        /* string attributes created by cdb_importCSV hold 64 bytes */

static const struct { const char* name; cDB_Datatype type; } format_names[] = {
    {"bool", CDB_BOOL},
    {"i32", CDB_INT32},     {"int32", CDB_INT32},     {"int", CDB_INT32},
    {"i64", CDB_INT64},     {"int64", CDB_INT64},     {"long", CDB_INT64},
    {"u32", CDB_UINT32},    {"uint32", CDB_UINT32},
    {"u64", CDB_UINT64},    {"uint64", CDB_UINT64},
    {"f32", CDB_FLOAT32},   {"float32", CDB_FLOAT32}, {"float", CDB_FLOAT32},
    {"f64", CDB_FLOAT64},   {"float64", CDB_FLOAT64}, {"double", CDB_FLOAT64},
    {"str", CDB_STRING},    {"string", CDB_STRING},   {"text", CDB_STRING},
};

static const char* type_label(cDB_Datatype t)
{
    static const char* labels[] = {"custom", "bool", "int32", "int64", "uint32", "uint64", "float32", "float64", "string"};
    return labels[t];
}

/* Parse one RFC 4180 field (quotes and "" escapes) into out.
 * Returns 1 if another field follows on this line, 0 at the end of the line. */
static int csv_next_field(const char** pp, char* out, size_t cap)
{
    const char* p = *pp;
    size_t n = 0;

    if (*p == '"') {
        p++;
        while (*p) {
            if (*p == '"') {
                if (p[1] == '"') { if (n + 1 < cap) out[n++] = '"'; p += 2; continue; }
                p++;
                break;
            }
            if (n + 1 < cap) out[n++] = *p;
            p++;
        }
    } else {
        while (*p && *p != ',' && *p != '\r' && *p != '\n') {
            if (n + 1 < cap) out[n++] = *p;
            p++;
        }
    }
    out[n] = '\0';
    while (*p && *p != ',' && *p != '\r' && *p != '\n') p++; /* junk after a closing quote */

    int more = (*p == ',');
    if (more) p++;
    *pp = p;
    return more;
}

/* Number of attribute columns (CSV columns - 1; column 1 is the record name), or -1 */
static int csv_attr_count(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) { perror(filename); return -1; }

    char line[CSV_LINE_MAX], field[CSV_LINE_MAX];
    if (!fgets(line, sizeof line, f)) {
        fprintf(stderr, "Error: %s is empty.\n", filename);
        fclose(f);
        return -1;
    }
    fclose(f);

    const char* p = (strncmp(line, "\xEF\xBB\xBF", 3) == 0) ? line + 3 : line; /* skip BOM */
    int cols = 0;
    while (1) { int more = csv_next_field(&p, field, sizeof field); cols++; if (!more) break; }
    if (cols < 2) {
        fprintf(stderr, "Error: need a header with at least 2 columns (first column = record name).\n");
        return -1;
    }
    return cols - 1;
}

/* Parse "string,float64,int64" into exactly `expected` datatypes. */
static bool parse_format(const char* spec, cDB_Datatype* out, uint32_t expected)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s", spec);

    uint32_t n = 0;
    for (char* tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        while (isspace((unsigned char)*tok)) tok++;
        char* e = tok + strlen(tok);
        while (e > tok && isspace((unsigned char)e[-1])) *--e = '\0';

        bool found = false;
        for (size_t i = 0; i < sizeof format_names / sizeof *format_names && !found; i++) {
            const char *a = tok, *b = format_names[i].name;
            while (*a && tolower((unsigned char)*a) == *b) { a++; b++; }
            if (!*a && !*b) {
                found = true;
                if (n < expected) out[n] = format_names[i].type;
            }
        }
        if (!found) {
            fprintf(stderr, "Error: unknown type \"%s\". Use: bool, i32, i64, u32, u64, f32, f64, str.\n", tok);
            return false;
        }
        n++;
    }
    if (n != expected) {
        fprintf(stderr, "Error: format lists %u type(s) but the CSV has %u attribute column(s) "
                        "(column 1 is the record name, so don't list it).\n", n, expected);
        return false;
    }
    return true;
}

/* Does text s fit the declared datatype? (cdb_importCSV would silently store 0 otherwise) */
static bool csv_value_ok(cDB_Datatype t, const char* s)
{
    char* end;
    const char* first = s;
    while (isspace((unsigned char)*first)) first++;
    errno = 0;

    switch (t) {
        case CDB_BOOL:
            return !strcmp(s, "true") || !strcmp(s, "false") || !strcmp(s, "1") || !strcmp(s, "0");
        case CDB_INT32:
        case CDB_INT64: {
            long long v = strtoll(s, &end, 10);
            if (end == s || errno == ERANGE) return false;
            while (isspace((unsigned char)*end)) end++;
            return *end == '\0' && (t == CDB_INT64 || (v >= INT32_MIN && v <= INT32_MAX));
        }
        case CDB_UINT32:
        case CDB_UINT64: {
            if (*first == '-') return false;
            unsigned long long v = strtoull(s, &end, 10);
            if (end == s || errno == ERANGE) return false;
            while (isspace((unsigned char)*end)) end++;
            return *end == '\0' && (t == CDB_UINT64 || v <= UINT32_MAX);
        }
        case CDB_FLOAT32:
        case CDB_FLOAT64:
            strtod(s, &end);
            if (end == s) return false;
            while (isspace((unsigned char)*end)) end++;
            return *end == '\0';
        default: /* CDB_STRING */
            return strlen(s) <= CSV_STR_MAX;
    }
}

/* Check every row against the declared format so nothing is silently stored wrong */
static bool csv_validate(const char* filename, const cDB_Datatype* fmt, uint32_t attrs)
{
    FILE* f = fopen(filename, "r");
    if (!f) { perror(filename); return false; }

    char line[CSV_LINE_MAX], field[CSV_LINE_MAX];
    uint64_t rows = 0, line_no = 1;
    bool ok = fgets(line, sizeof line, f) != NULL; /* header */

    while (ok && fgets(line, sizeof line, f)) {
        line_no++;
        if (!strchr(line, '\n') && !feof(f)) {
            fprintf(stderr, "Error: line %llu is longer than %d characters.\n", (unsigned long long)line_no, CSV_LINE_MAX - 1);
            ok = false;
            break;
        }
        const char* q = line;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q == '\0') continue; /* blank line (cdb_importCSV skips these too) */
        rows++;

        q = line;
        int more = csv_next_field(&q, field, sizeof field); /* record name */
        if (strlen(field) > CDB_STD_STRLEN - 1) {
            fprintf(stderr, "Error: line %llu: record name is longer than %d characters.\n", (unsigned long long)line_no, CDB_STD_STRLEN - 1);
            ok = false;
            break;
        }
        for (uint32_t j = 0; j < attrs; j++) {
            if (more) more = csv_next_field(&q, field, sizeof field); else field[0] = '\0';
            if (field[0] == '\0') continue; /* empty cell is stored as 0 / "" */
            if (!csv_value_ok(fmt[j], field)) {
                fprintf(stderr, "Error: line %llu, column %u: \"%.40s\" is not a valid %s%s.\n",
                        (unsigned long long)line_no, j + 2, field, type_label(fmt[j]),
                        fmt[j] == CDB_STRING ? " (max 63 characters)" : "");
                ok = false;
                break;
            }
        }
    }
    fclose(f);

    if (ok && rows == 0) { fprintf(stderr, "Error: %s has no data rows.\n", filename); ok = false; }
    return ok;
}

int convert_to_cdb(const char* filename, const char* format_spec, const char* output_cdb)
{
    /* default output: input name with .cdb instead of .csv */
    char out_buf[CDB_STD_STRLEN + 64];
    const char* out_filename = output_cdb;
    if (!out_filename || out_filename[0] == '\0') {
        snprintf(out_buf, sizeof out_buf, "%s", filename);
        char* dot = strrchr(out_buf, '.');
        char* slash = strrchr(out_buf, '/');
        char* bslash = strrchr(out_buf, '\\');
        if (dot && dot > slash && dot > bslash) *dot = '\0';
        strncat(out_buf, ".cdb", sizeof out_buf - strlen(out_buf) - 1);
        out_filename = out_buf;
    }
    if (strlen(out_filename) >= 65) { /* cdb_save takes filename[65] */
        fprintf(stderr, "Error: output path is longer than 64 characters: %s\n", out_filename);
        return 1;
    }

    int attrs = csv_attr_count(filename);
    if (attrs < 0) return 1;

    cDB_Datatype* format = malloc((size_t)attrs * sizeof *format);
    if (!format) {
        fprintf(stderr, "Error: out of memory.\n");
        return 1;
    }
    if (!parse_format(format_spec, format, (uint32_t)attrs) || !csv_validate(filename, format, (uint32_t)attrs)) {
        free(format);
        return 1;
    }

    cDB_Database* db = cdb_Create();
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database.\n");
        free(format);
        return 1;
    }

    int rc = 1;
    if (!cdb_importCSV(db, filename, format)) {
        fprintf(stderr, "Error: Could not import CSV: %s\n", filename);
    } else if (!cdb_save(db, out_filename)) {
        fprintf(stderr, "Error: Failed to save %s\n", out_filename);
    } else {
        printf("Converted %s to %s\n", filename, out_filename);
        rc = 0;
    }

    if (rc != 0) db->isSaved = true; /* don't let cdb_Destroy write a stray file */
    free(format);
    cdb_Destroy(db);
    return rc;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename.cdb>\n"
                        "       %s --view-t <filename.cdb>\n"
                        "       %s --export-csv <filename.cdb> [output.csv]\n"
                        "       %s --import-csv <filename.csv> <format> [output.cdb]\n"
                        "         format = one type per column after the first, e.g. string,f64,i32\n"
                        "         types: bool i32 i64 u32 u64 f32 f64 str\n",
                argv[0], argv[0], argv[0], argv[0]);
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
    else if(strcmp(config, "--import-csv") == 0)
    {
        if (argc < 4) {
            fprintf(stderr, "Error: Missing format for --import-csv (e.g. string,f64,i32)\n");
            return 1;
        }
        const char* output_cdb = (argc > 4) ? argv[4] : NULL;
        return convert_to_cdb(filename, argv[3], output_cdb);
    }
    else
    {
        fprintf(stderr, "Unknown option: %s\n", config);
        return 1;
    }
}