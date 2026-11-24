/* grading_fsm.c - cDB grading system written as a finite state machine
 *
 * Input : .cdb (attributes name, score, optional id) or .csv with header "id,name,score"
 * Output: .csv via cdb_exportCSV (columns RECORD=ID, Name, Grade)
 *         If the input was a .csv, the graded table is also saved to a .cdb
 *         which becomes the working file (changes are saved there automatically).
 *
 * How it runs: main() keeps one variable, the current State. Every state is a
 * function that does its job and returns the next State. S_END stops the loop.
 *
 *   S_LOAD -> S_MENU -> S_SHOW / S_STATS / S_QUERY / S_WRITE / S_GRADE / S_ADD / S_SORTFILE
 *                ^____________________________|  (every state returns to S_MENU)
 *   S_ADD returns to itself until an empty ID is entered.
 */
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include "../include/cDB.h"

/* ===================================================================== */
/*  Constants                                                            */
/* ===================================================================== */
#define NAME_LEN     129
#define ID_LEN       21
#define GRADE_LEN    4
#define LINE_LEN     512
#define FILE_LEN     65  /* cDB file names: 64 chars + '\0' */
#define EXTRA_SLOTS  100 /* free record slots kept for students added by hand */
#define N_CUTS       7
#define UI_WIDTH     66  /* width of banners, section rules and bars */
#define BAR_WIDTH    30  /* longest bar in the grade distribution */

enum { COL_NAME, COL_ID, COL_SCORE, COL_TSCORE, COL_GRADE, N_COLS };

typedef enum {
    S_LOAD, S_MENU, S_SHOW, S_STATS, S_QUERY, S_WRITE, S_GRADE, S_ADD, S_SORTFILE, S_END
} State;

static const char*  grade_names[N_CUTS + 1] = { "A", "B+", "B", "C+", "C", "D+", "D", "F" };
static const double std_cuts[N_CUTS]        = { 80, 75, 70, 65, 60, 55, 50 };
static const double t_cuts[N_CUTS]          = { 65, 60, 55, 50, 45, 40, 35 };

/* menu digit -> state */
static const State menu_next[] = { S_END, S_SHOW, S_STATS, S_QUERY, S_WRITE, S_GRADE, S_ADD, S_SORTFILE };

/* ===================================================================== */
/*  Program data (the machine's memory)                                  */
/* ===================================================================== */
static cDB_Database* db;
static cDB_Attribute A_name, A_id, A_score, A_tscore, A_grade;
static uint64_t count, cap;          /* students stored / record slots */
static bool     norm;                /* false = fixed cut-offs, true = T-score */
static double   mean, sd, mn, mx;    /* score statistics */

static char        input_file[LINE_LEN];
static char        work_file[FILE_LEN]; /* working .cdb when input was a .csv ("" = none) */
static int         exit_code;
static const char* want;                /* grade used by the select filter */

/* ===================================================================== */
/*  Small helpers                                                        */
/* ===================================================================== */
static char* trim(char* s) {
    while (isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

/* prompt and read one trimmed line; returns 0 on end of input */
static int read_line(const char* prompt, char* buf, size_t n) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) return 0;
    char* t = trim(buf);
    memmove(buf, t, strlen(t) + 1);
    return 1;
}

static int same_text(const char* a, const char* b) { /* case-insensitive equality */
    for (; *a && tolower((unsigned char)*a) == tolower((unsigned char)*b); a++, b++) {}
    return !*a && !*b;
}

static int has_ext(const char* f, const char* ext) {
    size_t l = strlen(f), e = strlen(ext);
    return l >= e && same_text(f + l - e, ext);
}

static int file_exists(const char* f) {
    FILE* fp = fopen(f, "rb");
    if (fp) fclose(fp);
    return fp != NULL;
}

/* read one cell as text, whatever its datatype */
static char* cell(cDB_Database* d, cDB_Record r, cDB_Attribute* a, char* buf) {
    uint64_t n = cdb_format_value(d, r, a, buf);
    buf[n < LINE_LEN ? n : LINE_LEN - 1] = '\0';
    return trim(buf);
}

static cDB_Attribute find_attr(cDB_Database* d, const char* w) { /* name, Name, NAME */
    char s[CDB_STD_STRLEN];
    cDB_Attribute a = { 0 };
    for (int v = 0; v < 3 && !a.isValid; v++) {
        snprintf(s, sizeof s, "%s", w);
        for (char* p = s; *p; p++)
            *p = (char)((v == 2 || (v == 1 && p == s)) ? toupper((unsigned char)*p) : tolower((unsigned char)*p));
        a = cdb_getAttribute(d, s);
    }
    return a;
}

static State fail(void) { exit_code = 1; return S_END; }

/* ===================================================================== */
/*  Terminal output style                                                */
/* ===================================================================== */
static void rule(char fill, int n) { while (n-- > 0) putchar(fill); }

/* boxed program title */
static void banner(const char* text) {
    int pad = UI_WIDTH - 2 - (int)strlen(text);
    printf("\n+"); rule('-', UI_WIDTH - 2); printf("+\n");
    printf("|%*s%s%*s|\n", pad / 2, "", text, pad - pad / 2, "");
    printf("+"); rule('-', UI_WIDTH - 2); printf("+\n");
}

/* section title: --- Title ------------------------ */
static void section(const char* text) {
    printf("\n--- %s ", text);
    rule('-', UI_WIDTH - 5 - (int)strlen(text));
    printf("\n");
}

/* tagged one-line messages: [ok] success, [!!] problem, [..] information */
static void tagged(const char* tag, const char* fmt, va_list ap) {
    printf("  %s ", tag);
    vprintf(fmt, ap);
    printf("\n");
}
static void ui_ok(const char* fmt, ...)   { va_list ap; va_start(ap, fmt); tagged("[ok]", fmt, ap); va_end(ap); }
static void ui_warn(const char* fmt, ...) { va_list ap; va_start(ap, fmt); tagged("[!!]", fmt, ap); va_end(ap); }
static void ui_info(const char* fmt, ...) { va_list ap; va_start(ap, fmt); tagged("[..]", fmt, ap); va_end(ap); }

/* ===================================================================== */
/*  Data: table, students, grades                                        */
/* ===================================================================== */
static bool create_table(uint64_t records) {
    cap = records + EXTRA_SLOTS;
    if (!cdb_CreateTable(db, "grades", (uint32_t)cap, N_COLS)) return false;
    A_name   = cdb_CreateAttribute(db, "Name",    CDB_STRING,  NAME_LEN);
    A_id     = cdb_CreateAttribute(db, "ID",      CDB_STRING,  ID_LEN);
    A_score  = cdb_CreateAttribute(db, "Score",   CDB_FLOAT64, 0);
    A_tscore = cdb_CreateAttribute(db, "T-Score", CDB_FLOAT64, 0);
    A_grade  = cdb_CreateAttribute(db, "Grade",   CDB_STRING,  GRADE_LEN);
    return A_grade.isValid;
}

/* returns NULL on success, otherwise the reason the student was rejected */
static const char* add_student(const char* name, const char* id, double score) {
    if (!*name || !*id)                                   return "empty name or ID";
    if (strlen(name) >= NAME_LEN || strlen(id) >= ID_LEN) return "name or ID too long";
    if (!(score >= 0 && score <= 100))                    return "score out of range 0-100";
    if (count >= cap)                                     return "table is full";
    if (cdb_get_record(db, id) != UINT64_MAX)             return "duplicate ID";

    cDB_Record r = count++;
    cdb_set_recordName(db, r, id); /* record name = student ID */
    cdb_set_string(db, r, &A_name, name);
    cdb_set_string(db, r, &A_id, id);
    cdb_set_f64(db, r, &A_score, score);
    return NULL;
}

/* recompute statistics, then every student's T-score (norm mode) and grade */
static void apply_grades(void) {
    double sum = 0, sq = 0;
    mn = 100; mx = 0;
    for (cDB_Record r = 0; r < count; r++) {
        double s = cdb_get_f64(db, r, &A_score);
        mn = fmin(mn, s); mx = fmax(mx, s);
        sum += s; sq += s * s;
    }
    mean = sum / count;
    sd = sqrt(fmax(0, sq / count - mean * mean));

    const double* cuts = norm ? t_cuts : std_cuts;
    for (cDB_Record r = 0; r < count; r++) {
        double v = cdb_get_f64(db, r, &A_score);
        if (norm) {
            v = sd == 0 ? 50 : 50 + 10 * (v - mean) / sd;
            cdb_set_f64(db, r, &A_tscore, v);
        }
        int g = 0;
        while (g < N_CUTS && v < cuts[g]) g++;
        cdb_set_string(db, r, &A_grade, grade_names[g]);
    }
}

static bool has_grade(cDB_Database* d, cDB_Record r, void* u) { /* select predicate */
    (void)u;
    return strcmp(cdb_get_string(d, r, &A_grade), want) == 0;
}

/* ===================================================================== */
/*  Loading (.cdb or .csv)                                               */
/* ===================================================================== */
/* CSV column 1 = record name (the ID); format[] = one datatype per remaining column */
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

/* copy every valid student of the opened source database into our own table */
static bool import_students(cDB_Database* src, const char* file) {
    /* ID = "id" attribute if there is one, otherwise the record name (CSV column 1) */
    cDB_Attribute ia = find_attr(src, "id"), na = find_attr(src, "name"), sa = find_attr(src, "score");
    uint64_t n = src->table->recordCapacity;
    int skipped = 0;

    if (!na.isValid || !sa.isValid) {
        ui_warn("\"%s\" must have the columns name and score (CSV: id,name,score).", file);
        return false;
    }
    if (!create_table(n)) return false;

    for (cDB_Record r = 0; r < n; r++) {
        char nb[LINE_LEN], ib[LINE_LEN], sb[LINE_LEN];
        char* name = cell(src, r, &na, nb), * sc = cell(src, r, &sa, sb), * id;
        if (ia.isValid) id = cell(src, r, &ia, ib);
        else { snprintf(ib, sizeof ib, "%s", src->table->recordNames[r]); id = trim(ib); }
        if (!*name) continue; /* empty slot */

        char* end;
        double score = strtod(sc, &end);
        const char* why = (end == sc || !isfinite(score)) ? "score is not a number"
                                                          : add_student(name, id, score);
        if (why) { ui_warn("Record %llu skipped: %s", (unsigned long long)r, why); skipped++; }
    }
    ui_ok("Loaded %llu student(s) from %s (skipped %d).", (unsigned long long)count, file, skipped);
    return count > 0;
}

static bool load(const char* file) {
    bool is_cdb = has_ext(file, ".cdb"), ok = false;
    cDB_Datatype* fmt = NULL;

    if (!is_cdb && !has_ext(file, ".csv")) { ui_warn("Use a .cdb or .csv file."); return false; }
    if (!is_cdb && !(fmt = csv_format(file))) { ui_warn("Could not read the header of \"%s\".", file); return false; }

    cDB_Database* src = cdb_Create();
    bool opened = src && (is_cdb ? cdb_open(src, file) : cdb_importCSV(src, file, fmt));
    if (opened) ok = import_students(src, file);
    else ui_warn("Could not open \"%s\".", file);

    if (src) { src->isSaved = true; cdb_Destroy(src); } /* never auto-save the input */
    free(fmt);
    return ok;
}

/* ===================================================================== */
/*  Working .cdb (only when the input was a .csv)                        */
/* ===================================================================== */
/* save the graded table to <name>.cdb (or another name if taken) and make it the working file */
static void emit_cdb(const char* csv) {
    char out[LINE_LEN];
    snprintf(out, sizeof out, "%s", csv);
    strcpy(out + strlen(out) - 4, ".cdb"); /* same length as ".csv" */

    while (file_exists(out) || strlen(out) >= FILE_LEN || !has_ext(out, ".cdb")) {
        ui_warn("Cannot use \"%s\" (already exists, too long or not .cdb).", out);
        if (!read_line("  Another .cdb file name (empty = skip): ", out, sizeof out) || !*out) {
            ui_info("No .cdb written; changes will not be saved.");
            return;
        }
    }
    if (cdb_save(db, out)) {
        snprintf(work_file, sizeof work_file, "%s", out);
        ui_ok("Working file is now \"%s\"; changes are saved there.", out);
    }
    else ui_warn("Could not write \"%s\".", out);
}

/* called after every action that changes data */
static void autosave(void) {
    if (*work_file && !cdb_save(db, work_file))
        ui_warn("Could not save \"%s\".", work_file);
}

/* ===================================================================== */
/*  Permanent sort of the table                                          */
/* ===================================================================== */
/* Sort a projection of the students, then rearrange the real records to that
 * order with swaps. Only slots 0..count-1 move, so empty spare slots stay at the end. */
static bool sort_table(cDB_Attribute* by, cDB_SortDirection dir) {
    const uint64_t col = COL_NAME;
    cDB_Projection* p = cdb_CreateProjection(db, NULL, count, &col, 1);
    uint64_t* at  = malloc(count * sizeof * at);   /* at[slot]  = record now in that slot */
    uint64_t* pos = malloc(count * sizeof * pos);  /* pos[rec]  = slot where that record is */
    bool ok = p && at && pos;

    if (ok) {
        cdb_sort_Projection(db, p, by, dir);
        for (uint64_t i = 0; i < count; i++) at[i] = pos[i] = i;
        for (uint64_t i = 0; i < count; i++) {
            uint64_t want_rec = p->projectedRecords[i], j = pos[want_rec], displaced = at[i];
            if (j == i) continue;
            cdb_swapRecord(db, i, j);
            at[i] = want_rec;  at[j] = displaced;
            pos[want_rec] = i; pos[displaced] = j;
        }
    }
    if (p) cdb_freeProjection(p);
    free(at);
    free(pos);
    return ok;
}

/* ===================================================================== */
/*  Output helpers                                                       */
/* ===================================================================== */
/* ask how to sort; returns false for "as stored" or invalid input (= no sorting) */
static bool choose_sort(cDB_Attribute** by, cDB_SortDirection* dir) {
    char c[LINE_LEN];
    printf("  Sort by:\n"
           "    1  As stored        2  Name A-Z        3  Name Z-A\n"
           "    4  Score high-low   5  Score low-high\n");
    if (!read_line("  Choose [1-5] > ", c, sizeof c) || c[0] < '2' || c[0] > '5' || c[1]) return false;
    *by  = c[0] <= '3' ? &A_name : &A_score;
    *dir = (c[0] == '2' || c[0] == '5') ? CDB_SORTDIR_ASCEND : CDB_SORTDIR_DESCEND;
    return true;
}

/* print records (NULL = all); ask_sort: ask how to sort the projection first */
static void show(cDB_Record* recs, uint64_t n, bool ask_sort) {
    static cDB_PrintStyle style = { '+', '+', '+', '+', '|', '-' }; /* plain ASCII table; pass NULL for cDB's default */
    static const uint64_t fixed_cols[] = { COL_NAME, COL_SCORE, COL_GRADE };
    static const uint64_t norm_cols[]  = { COL_NAME, COL_SCORE, COL_TSCORE, COL_GRADE }; /* T-Score only in norm mode */
    cDB_Projection* p = norm ? cdb_CreateProjection(db, recs, n, norm_cols, 4)
                             : cdb_CreateProjection(db, recs, n, fixed_cols, 3);
    if (!p) { ui_warn("Failed to create projection."); return; }

    if (ask_sort) {
        cDB_Attribute* by;
        cDB_SortDirection dir;
        if (choose_sort(&by, &dir)) cdb_sort_Projection(db, p, by, dir);
        else ui_info("Keeping stored order.");
    }
    printf("\n");
    cdb_printProjection(db, p, false, true, &style);
    cdb_freeProjection(p);
}

/* ===================================================================== */
/*  States - each returns the next state                                 */
/* ===================================================================== */
/* ask which grading method to use; regrades everyone if it changed */
static void choose_method(void) {
    char l[LINE_LEN];
    printf("  Grading method:\n"
           "    1  Fixed cut-offs     A>=80 B+>=75 B>=70 C+>=65 C>=60 D+>=55 D>=50\n"
           "    2  Norm-referenced    T = 50 + 10*(score-mean)/SD\n"
           "                          A>=65 B+>=60 B>=55 C+>=50 C>=45 D+>=40 D>=35\n"
           "  Current: %s\n\n", norm ? "norm-referenced" : "fixed cut-offs");
    if (read_line("  Choose [1-2] > ", l, sizeof l) && (!strcmp(l, "1") || !strcmp(l, "2"))) {
        bool new_norm = l[0] == '2';
        if (new_norm != norm) {
            norm = new_norm;
            apply_grades();
            autosave();
        }
        ui_ok("Grading method: %s.", norm ? "norm-referenced" : "fixed cut-offs");
    }
    else ui_info("Keeping %s.", norm ? "norm-referenced" : "fixed cut-offs");
}

static State st_load(void) {
    if (!*input_file && (!read_line("Input file name (.cdb or .csv) > ", input_file, sizeof input_file) || !*input_file))
        return fail();
    if (strlen(input_file) >= FILE_LEN) { ui_warn("File name too long (max 64)."); return fail(); }

    db = cdb_Create();
    if (!db || !load(input_file)) return fail();

    apply_grades();
    if (has_ext(input_file, ".csv")) emit_cdb(input_file);
    return S_MENU;
}

static State st_menu(void) {
    char l[LINE_LEN];
    banner("cGrading");
    printf("  1  Show all students and grades\n"
           "  2  Statistics\n"
           "  3  Query by student ID\n"
           "  4  Write output file (.csv)\n"
           "  5  Students with a grade\n"
           "  6  Add students\n"
           "  7  Sort entire file (permanent)\n"
           "  0  Exit\n\n");
    if (!read_line("  Choose [0-7] > ", l, sizeof l)) return S_END;
    if (l[0] >= '0' && l[0] <= '7' && !l[1]) return menu_next[l[0] - '0'];
    ui_warn("Invalid choice.");
    return S_MENU;
}

static State st_show(void) {
    section("All students");
    choose_method();
    show(NULL, count, true);
    return S_MENU;
}

static State st_stats(void) {
    uint64_t per_grade[N_CUTS + 1], top = 1;
    for (int g = 0; g <= N_CUTS; g++) {
        want = grade_names[g];
        cdb_select(db, has_grade, NULL);
        per_grade[g] = cdb_getResult(db)->resultCount;
        if (per_grade[g] > top) top = per_grade[g];
    }
    cdb_clearResult(db);

    section("Statistics");
    printf("  Students : %llu\n  Mean     : %.2f\n  SD       : %.2f\n  Min      : %.2f\n  Max      : %.2f\n",
           (unsigned long long)count, mean, sd, mn, mx);
    printf("\n  Grade distribution\n");
    for (int g = 0; g <= N_CUTS; g++) {
        printf("  %-2s | ", grade_names[g]);
        int len = (int)(per_grade[g] * BAR_WIDTH / top);
        rule('#', (per_grade[g] && !len) ? 1 : len); /* at least one # for a non-zero count */
        printf("%*s%llu\n", per_grade[g] ? 1 : 0, "", (unsigned long long)per_grade[g]);
    }
    return S_MENU;
}

static State st_query(void) {
    char l[LINE_LEN];
    section("Query by student ID");
    if (!read_line("  Student ID > ", l, sizeof l)) return S_MENU;

    cDB_Record r = strlen(l) < CDB_STD_STRLEN ? cdb_get_record(db, l) : UINT64_MAX;
    if (r == UINT64_MAX) ui_warn("Student ID '%s' not found.", l);
    else show(&r, 1, false);
    return S_MENU;
}

static State st_write(void) {
    char out[LINE_LEN];
    section("Write output file");
    if (!read_line("  Output file name (.csv) > ", out, sizeof out) || !has_ext(out, ".csv")
        || !strcmp(out, input_file) || strlen(out) >= FILE_LEN) {
        ui_warn("Need a .csv file name (max 64 chars) different from the input file.");
        return S_MENU;
    }
    const uint64_t cols[2] = { COL_NAME, COL_GRADE }; /* the RECORD column cdb_exportCSV adds is the student ID */
    cDB_Projection* p = cdb_CreateProjection(db, NULL, count, cols, 2);
    if (!p) { ui_warn("Failed to create projection."); return S_MENU; }

    if (cdb_exportCSV(db, p, out)) ui_ok("Wrote \"%s\".", out);
    else ui_warn("Could not write \"%s\".", out);
    cdb_freeProjection(p);
    return S_MENU;
}

static State st_grade(void) {
    char l[LINE_LEN] = "";
    want = NULL;
    section("Students with a grade");
    if (read_line("  Grade (A, B+, B, C+, C, D+, D, F) > ", l, sizeof l))
        for (int g = 0; g <= N_CUTS; g++)
            if (same_text(l, grade_names[g])) want = grade_names[g];
    if (!want) { ui_warn("Invalid grade '%s'.", l); return S_MENU; }

    cdb_select(db, has_grade, NULL);
    const cDB_Result* res = cdb_getResult(db);
    if (res->resultCount == 0) ui_info("No students with grade %s.", want);
    else show(res->matchingRecords, res->resultCount, true);
    cdb_clearResult(db);
    return S_MENU;
}

/* one student per visit, entered as a single line "ID,Name,Score";
 * stays in S_ADD until an empty line is entered */
static State st_add(void) {
    char line[LINE_LEN], id[LINE_LEN], name[LINE_LEN];
    double score;
    int used = 0;

    section("Add student");
    printf("  Format: ID,Name,Score     e.g. 6501234,Somchai Jaidee,78.5\n");
    if (!read_line("  Student (empty to finish) > ", line, sizeof line) || !*line) return S_MENU;

    if (sscanf(line, " %511[^,],%511[^,],%lf %n", id, name, &score, &used) != 3 || line[used]) {
        ui_warn("Not added: use the format ID,Name,Score");
        return S_ADD;
    }
    const char* why = isfinite(score) ? add_student(trim(name), trim(id), score) : "score is not a number";
    if (why) { ui_warn("Not added: %s", why); return S_ADD; }

    apply_grades(); /* mean/SD changed, so regrade everyone */
    autosave();
    cDB_Record r = count - 1;
    ui_ok("Added:");
    show(&r, 1, false);
    return S_ADD;
}

/* sort the records for good and rewrite the .cdb, only after the user confirms */
static State st_sortfile(void) {
    char l[LINE_LEN];
    cDB_Attribute* by;
    cDB_SortDirection dir;
    const char* target = *work_file ? work_file : (has_ext(input_file, ".cdb") ? input_file : NULL);

    section("Sort entire file");
    if (!target) {
        ui_warn("No .cdb file to sort (the input was a .csv and no .cdb was written).");
        return S_MENU;
    }
    if (!choose_sort(&by, &dir)) { ui_info("File not changed."); return S_MENU; }

    printf("\n  This permanently reorders the records and rewrites \"%s\"\n"
           "  with the sorted table (columns ID, Name, Score, T-Score, Grade).\n\n", target);
    if (!read_line("  Type yes to confirm (Anything else to cancel) > ", l, sizeof l) || !same_text(l, "yes")) {
        ui_info("Cancelled. File not changed.");
        return S_MENU;
    }

    if (!sort_table(by, dir)) { ui_warn("Sorting failed."); return S_MENU; }
    if (cdb_save(db, target)) ui_ok("Sorted and saved to \"%s\".", target);
    else ui_warn("Sorted in memory, but could not save \"%s\".", target);
    return S_MENU;
}

/* ===================================================================== */
/*  Main loop                                                            */
/* ===================================================================== */
static State (*const handler[])(void) = {
    [S_LOAD] = st_load,   [S_MENU]  = st_menu,  [S_SHOW] = st_show,
    [S_STATS] = st_stats, [S_QUERY] = st_query,
    [S_WRITE] = st_write, [S_GRADE] = st_grade, [S_ADD] = st_add,
    [S_SORTFILE] = st_sortfile,
};

int main(int argc, char** argv) {
    if (argc >= 2) snprintf(input_file, sizeof input_file, "%s", argv[1]);

    for (State st = S_LOAD; st != S_END; st = handler[st]()) {}

    if (db) { db->isSaved = true; cdb_Destroy(db); }
    return exit_code;
}