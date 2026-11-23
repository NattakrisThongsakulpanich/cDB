/* grading_short.c - compact grading system on cDB
 * Input : .cdb (attributes name, score, optional id) or .csv with header "id,name,score"
 *         (CSV column 1 becomes the record name = student ID, as cdb_importCSV expects)
 * Output: .csv via cdb_exportCSV (columns RECORD=ID, Name, Grade)
 */
#include <ctype.h>
#include <math.h>
#include "../include/cDB.h"

#define NAME_LEN 129
#define ID_LEN 21
#define GRADE_LEN 4
#define LINE_LEN 512
#define N_CUTS 7

enum { COL_NAME, COL_ID, COL_SCORE, COL_TSCORE, COL_GRADE, N_COLS };

static const char* grade_names[N_CUTS + 1] = { "A", "B+", "B", "C+", "C", "D+", "D", "F" };
static const double std_cuts[N_CUTS] = { 80, 75, 70, 65, 60, 55, 50 };
static const double t_cuts[N_CUTS] = { 65, 60, 55, 50, 45, 40, 35 };

typedef struct {
    cDB_Database* db;
    cDB_Attribute name, id, score, tscore, grade;
    uint64_t count;
    bool norm; /* false = fixed cut-offs, true = T-score (mean/SD) */
    double mean, sd, min, max;
} Book;

/* ---------- helpers ---------- */
static char* trim(char* s) {
    while (isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

static int read_line(const char* prompt, char* buf, size_t n) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) return 0;
    char* t = trim(buf);
    memmove(buf, t, strlen(t) + 1);
    return 1;
}

static int same_text(const char* a, const char* b) {
    for (; *a && tolower((unsigned char)*a) == tolower((unsigned char)*b); a++, b++) {}
    return !*a && !*b;
}

static int has_ext(const char* f, const char* ext) {
    size_t l = strlen(f), e = strlen(ext);
    return l >= e && same_text(f + l - e, ext);
}

/* ---------- table ---------- */
static int create_table(Book* b, uint64_t cap) {
    if (!cdb_CreateTable(b->db, "grades", (uint32_t)cap, N_COLS)) return 0;
    b->name = cdb_CreateAttribute(b->db, "Name", CDB_STRING, NAME_LEN);
    b->id = cdb_CreateAttribute(b->db, "ID", CDB_STRING, ID_LEN);
    b->score = cdb_CreateAttribute(b->db, "Score", CDB_FLOAT64, 0);
    b->tscore = cdb_CreateAttribute(b->db, "T-Score", CDB_FLOAT64, 0);
    b->grade = cdb_CreateAttribute(b->db, "Grade", CDB_STRING, GRADE_LEN);
    return b->grade.isValid;
}

/* returns NULL on success, otherwise the reason the student was rejected */
static const char* add_student(Book* b, const char* name, const char* id, double score) {
    if (!*name || !*id) return "empty name or ID";
    if (strlen(name) >= NAME_LEN || strlen(id) >= ID_LEN) return "name or ID too long";
    if (!(score >= 0 && score <= 100)) return "score out of range 0-100";
    if (cdb_get_record(b->db, id) != UINT64_MAX) return "duplicate ID";

    cDB_Record r = b->count++;
    cdb_set_recordName(b->db, r, id); /* record name = student ID */
    cdb_set_string(b->db, r, &b->name, name);
    cdb_set_string(b->db, r, &b->id, id);
    cdb_set_f64(b->db, r, &b->score, score);
    return NULL;
}

/* ---------- loading ---------- */
static cDB_Attribute find_attr(cDB_Database* db, const char* w) { /* name, Name, NAME */
    char s[CDB_STD_STRLEN];
    cDB_Attribute a = { 0 };
    for (int v = 0; v < 3 && !a.isValid; v++) {
        snprintf(s, sizeof s, "%s", w);
        for (char* p = s; *p; p++)
            *p = (char)((v == 2 || (v == 1 && p == s)) ? toupper((unsigned char)*p) : tolower((unsigned char)*p));
        a = cdb_getAttribute(db, s);
    }
    return a;
}

/* cdb_importCSV layout: column 1 = record name (the student ID); every other column
 * becomes an attribute, and format[] holds one datatype per attribute (not per column).
 * Header example: id,name,score  ->  format = {STRING, FLOAT64} */
static cDB_Datatype* csv_format(const char* file) {
    FILE* f = fopen(file, "r");
    char head[LINE_LEN];
    if (!f) { perror(file); return NULL; }
    char* got = fgets(head, sizeof head, f);
    fclose(f);
    if (!got) return NULL;

    char* p = strncmp(head, "\xEF\xBB\xBF", 3) == 0 ? head + 3 : head; /* skip BOM */
    size_t cols = 1;
    for (char* c = p; *c; c++) cols += (*c == ',');
    if (cols < 2) return NULL;

    cDB_Datatype* fmt = malloc((cols - 1) * sizeof * fmt);
    for (size_t i = 0; fmt && i < cols; i++) {
        char* comma = strchr(p, ',');
        if (comma) *comma = '\0';
        char* w = trim(p);
        size_t l = strlen(w);
        if (l >= 2 && w[0] == '"' && w[l - 1] == '"') { w[l - 1] = '\0'; w++; }
        if (i > 0) fmt[i - 1] = same_text(w, "score") ? CDB_FLOAT64 : CDB_STRING;
        p = comma ? comma + 1 : p + strlen(p);
    }
    return fmt;
}

/* read one cell as text, whatever its datatype */
static char* cell(cDB_Database* db, cDB_Record r, cDB_Attribute* a, char* buf) {
    uint64_t n = cdb_format_value(db, r, a, buf);
    buf[n < LINE_LEN ? n : LINE_LEN - 1] = '\0';
    return trim(buf);
}

static int load(Book* b, const char* file) {
    int isCdb = has_ext(file, ".cdb"), ok = 0, skipped = 0;
    cDB_Datatype* fmt = NULL;

    if (!isCdb && !has_ext(file, ".csv")) { printf("Use a .cdb or .csv file.\n"); return 0; }
    if (!isCdb && !(fmt = csv_format(file))) {
        printf("Could not read the header of \"%s\".\n", file);
        return 0;
    }

    cDB_Database* src = cdb_Create();
    if (src && (isCdb ? cdb_open(src, file) : cdb_importCSV(src, file, fmt))) {
        /* ID = "id" attribute if there is one, otherwise the record name (CSV column 1) */
        cDB_Attribute ia = find_attr(src, "id"), na = find_attr(src, "name"), sa = find_attr(src, "score");
        uint64_t cap = src->table->recordCapacity;
        if (!na.isValid || !sa.isValid) {
            printf("\"%s\" must have the columns name and score (CSV: id,name,score).\n", file);
            for (uint64_t k = 0; k < 8; k++) { /* debug: what did the import create? */
                cDB_Attribute a = cdb_getAttribute_byIndex(src, k);
                if (!a.isValid) break;
                printf("  attribute %llu: \"%s\" (type %d)\n", (unsigned long long)k, a.name, (int)a.type);
            }
        }
        else if (create_table(b, cap)) {
            for (cDB_Record r = 0; r < cap; r++) {
                char n[LINE_LEN], i[LINE_LEN], s[LINE_LEN];
                char* name = cell(src, r, &na, n), * sc = cell(src, r, &sa, s), * id;
                if (ia.isValid) id = cell(src, r, &ia, i);
                else { snprintf(i, sizeof i, "%s", src->table->recordNames[r]); id = trim(i); }
                if (!*name) continue; /* empty slot */
                char* end;
                double score = strtod(sc, &end);
                const char* why = (end == sc || !isfinite(score)) ? "score is not a number"
                    : add_student(b, name, id, score);
                if (why) { printf("Record %llu skipped: %s\n", (unsigned long long)r, why); skipped++; }
            }
            ok = b->count > 0;
            printf("Loaded %llu student(s) from %s, skipped %d.\n", (unsigned long long)b->count, file, skipped);
        }
    }
    else {
        printf("Could not open \"%s\".\n", file);
    }
    if (src) { src->isSaved = true; cdb_Destroy(src); } /* never auto-save the input */
    free(fmt);
    return ok;
}

/* ---------- grading ---------- */
static void apply_grades(Book* b) {
    double sum = 0, sq = 0;
    b->min = 100; b->max = 0;
    for (cDB_Record r = 0; r < b->count; r++) {
        double s = cdb_get_f64(b->db, r, &b->score);
        b->min = fmin(b->min, s); b->max = fmax(b->max, s);
        sum += s; sq += s * s;
    }
    b->mean = sum / b->count;
    b->sd = sqrt(fmax(0, sq / b->count - b->mean * b->mean));

    for (cDB_Record r = 0; r < b->count; r++) {
        double v = cdb_get_f64(b->db, r, &b->score);
        if (b->norm) {
            v = b->sd == 0 ? 50 : 50 + 10 * (v - b->mean) / b->sd;
            cdb_set_f64(b->db, r, &b->tscore, v);
        }
        const double* cuts = b->norm ? t_cuts : std_cuts;
        int g = 0;
        while (g < N_CUTS && v < cuts[g]) g++;
        cdb_set_string(b->db, r, &b->grade, grade_names[g]);
    }
}

/* ---------- menu ---------- */
static void show(Book* b, cDB_Record* recs, uint64_t n) {
    uint64_t cols[4] = { COL_NAME, COL_SCORE, COL_TSCORE, COL_GRADE };
    if (!b->norm) { cols[2] = COL_GRADE; } /* hide T-Score unless norm-referenced */
    cDB_Projection* p = cdb_CreateProjection(b->db, recs, n, cols, b->norm ? 4 : 3);
    if (!p) { printf("Failed to create projection.\n"); return; }
    cdb_printProjection(b->db, p, false, true, NULL);
    cdb_freeProjection(p);
}

typedef struct { cDB_Attribute grade; const char* want; } Filter;

static bool has_grade(cDB_Database* db, cDB_Record r, void* u) {
    Filter* f = u;
    return strcmp(cdb_get_string(db, r, &f->grade), f->want) == 0;
}

static void stats(Book* b) {
    printf("Count: %llu\nMax: %.2f\nMean: %.2f\nMin: %.2f\nSD: %.2f\nDistribution:",
        (unsigned long long)b->count, b->max, b->mean, b->min, b->sd);
    for (int g = 0; g <= N_CUTS; g++) {
        Filter f = { b->grade, grade_names[g] };
        cdb_select(b->db, has_grade, &f);
        printf(" %s=%llu", grade_names[g], (unsigned long long)cdb_getResult(b->db)->resultCount);
    }
    cdb_clearResult(b->db);
    printf("\n");
}

static void change_mode(Book* b) {
    char l[LINE_LEN];
    printf("1) Fixed cut-offs (A>=80 B+>=75 B>=70 C+>=65 C>=60 D+>=55 D>=50)\n"
        "2) Norm-referenced (T-score = 50 + 10*(score-mean)/SD)\n");
    if (!read_line("Choose: ", l, sizeof l) || (strcmp(l, "1") && strcmp(l, "2"))) {
        printf("Grading mode not changed.\n");
        return;
    }
    b->norm = l[0] == '2';
    apply_grades(b);
    printf("Grades recalculated (%s).\n", b->norm ? "norm-referenced" : "fixed cut-offs");
}

static void query(Book* b) {
    char l[LINE_LEN];
    if (!read_line("Student ID: ", l, sizeof l)) return;
    cDB_Record r = strlen(l) < CDB_STD_STRLEN ? cdb_get_record(b->db, l) : UINT64_MAX;
    if (r == UINT64_MAX) printf("Student ID '%s' not found.\n", l);
    else show(b, &r, 1);
}

static void write_output(Book* b, const char* input) {
    char out[LINE_LEN];
    if (!read_line("Output file name (.csv): ", out, sizeof out) || !has_ext(out, ".csv") || !strcmp(out, input)) {
        printf("Need a .csv file name different from the input file.\n");
        return;
    }
    uint64_t cols[2] = { COL_NAME, COL_GRADE }; /* the RECORD column cdb_exportCSV adds is the student ID */
    cDB_Projection* p = cdb_CreateProjection(b->db, NULL, b->count, cols, 2);
    if (!p) { printf("Failed to create projection.\n"); return; }
    printf(cdb_exportCSV(b->db, p, out) ? "Wrote \"%s\".\n" : "Could not write \"%s\".\n", out);
    cdb_freeProjection(p);
}

static void by_grade(Book* b) {
    char l[LINE_LEN];
    if (!read_line("Grade (A, B+, B, C+, C, D+, D, F): ", l, sizeof l)) return;
    Filter f = { b->grade, NULL };
    for (int g = 0; g <= N_CUTS; g++) if (same_text(l, grade_names[g])) f.want = grade_names[g];
    if (!f.want) { printf("Invalid grade '%s'.\n", l); return; }

    cdb_select(b->db, has_grade, &f);
    const cDB_Result* res = cdb_getResult(b->db);
    if (res->resultCount == 0) printf("No students with grade %s.\n", f.want);
    else show(b, res->matchingRecords, res->resultCount);
    cdb_clearResult(b->db);
}

int main(int argc, char** argv) {
    char file[LINE_LEN], c[LINE_LEN];
    if (argc >= 2) snprintf(file, sizeof file, "%s", argv[1]);
    else if (!read_line("Input file name (.cdb or .csv): ", file, sizeof file) || !*file) return 1;

    Book b = { 0 };
    b.db = cdb_Create();
    int ok = b.db && load(&b, file);
    if (ok) apply_grades(&b);

    while (ok) {
        printf("\n 1) Show all students and grades\n 2) Statistics\n 3) Change grading mode\n"
            " 4) Query by student ID\n 5) Write output file\n 6) Students with a grade\n 0) Exit\n");
        if (!read_line("Choose: ", c, sizeof c) || !strcmp(c, "0")) break;
        switch (c[0] && !c[1] ? c[0] : '?') {
        case '1': show(&b, NULL, b.count); break;
        case '2': stats(&b); break;
        case '3': change_mode(&b); break;
        case '4': query(&b); break;
        case '5': write_output(&b, file); break;
        case '6': by_grade(&b); break;
        default: printf("Invalid choice.\n");
        }
    }
    if (b.db) { b.db->isSaved = true; cdb_Destroy(b.db); }
    return ok ? 0 : 1;
}