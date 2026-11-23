/* datagen.c - test-data generator for the grader
 * Usage: datagen <out.csv|out.cdb> [count=50] [mean=65] [sd=15] [seed=time]
 * Columns: id,name,score. Scores are normally distributed, clamped to 0-100.
 * The first rows are fixed boundary scores (100, 80, 75 ... 50, 49.99, 0)
 * so every grade cut-off gets tested.
 * Build: gcc datagen.c -o datagen -lm -I<cDB include> <libcDB>   (add -DNO_CDB for CSV only)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef NO_CDB
#include "../include/cDB.h"
#endif
 
static const char *first[] = {"Somchai", "Somying", "Anan", "Malee", "Preecha", "Suda", "Krit", "Nida",
                              "Thanawat", "Ploy", "Chai", "Mali", "Nattapong", "Kanya", "Arthit", "Pim"};
static const char *last[] = {"Srisuk", "Wongsa", "Chaiyo", "Boonmee", "Thongdee", "Saetang", "Jaidee", "Rattana"};
static const double edge[] = {100, 80, 75, 70, 65, 60, 55, 50, 49.99, 0};
 
#define N_FIRST (sizeof first / sizeof *first)
#define N_LAST (sizeof last / sizeof *last)
#define N_EDGE (sizeof edge / sizeof *edge)
 
static double normal(double mean, double sd) { /* Box-Muller */
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return mean + sd * sqrt(-2 * log(u)) * cos(2 * 3.14159265358979 * v);
}
 
static double make_score(unsigned i, double mean, double sd) {
    if (i < N_EDGE) return edge[i];
    double s = normal(mean, sd);
    s = s < 0 ? 0 : s > 100 ? 100 : s;
    return floor(s * 100 + 0.5) / 100; /* 2 decimals */
}
 
static void make_id(unsigned i, char *id) { snprintf(id, 21, "68%06u", i + 1); }
 
static void make_name(char *name) {
    snprintf(name, 64, "%s %s", first[rand() % N_FIRST], last[rand() % N_LAST]);
}
 
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <out.csv|out.cdb> [count=50] [mean=65] [sd=15] [seed]\n", argv[0]);
        return 1;
    }
    const char *out = argv[1];
    unsigned n = argc > 2 ? (unsigned)atoi(argv[2]) : 50;
    double mean = argc > 3 ? atof(argv[3]) : 65, sd = argc > 4 ? atof(argv[4]) : 15;
    srand(argc > 5 ? (unsigned)atoi(argv[5]) : (unsigned)time(NULL));
    if (n == 0 || n > 99999) { fprintf(stderr, "count must be 1-99999\n"); return 1; }
 
    size_t len = strlen(out);
    int isCdb = len > 4 && strcmp(out + len - 4, ".cdb") == 0;
    int isCsv = len > 4 && strcmp(out + len - 4, ".csv") == 0;
    char id[21], name[64];
 
    if (isCsv) {
        FILE *f = fopen(out, "w");
        if (!f) { perror(out); return 1; }
        fprintf(f, "id,name,score\n");
        for (unsigned i = 0; i < n; i++) {
            make_id(i, id);
            make_name(name);
            fprintf(f, "%s,%s,%.2f\n", id, name, make_score(i, mean, sd));
        }
        fclose(f);
#ifndef NO_CDB
    } else if (isCdb) {
        cDB_Database *db = cdb_Create();
        if (!db || !cdb_CreateTable(db, "students", n, 3)) { fprintf(stderr, "cDB error\n"); return 1; }
        cDB_Attribute aid = cdb_CreateAttribute(db, "id", CDB_STRING, 21);
        cDB_Attribute aname = cdb_CreateAttribute(db, "name", CDB_STRING, 64);
        cDB_Attribute ascore = cdb_CreateAttribute(db, "score", CDB_FLOAT64, 0);
        for (unsigned i = 0; i < n; i++) {
            make_id(i, id);
            make_name(name);
            cdb_set_recordName(db, i, id);
            cdb_set_string(db, i, &aid, id);
            cdb_set_string(db, i, &aname, name);
            cdb_set_f64(db, i, &ascore, make_score(i, mean, sd));
        }
        if (!cdb_save(db, out)) { fprintf(stderr, "Could not save %s\n", out); return 1; }
        cdb_Destroy(db);
#endif
    } else {
        fprintf(stderr, "Output must end with .csv%s\n",
#ifndef NO_CDB
                " or .cdb"
#else
                ""
#endif
        );
        return 1;
    }
    printf("Wrote %u students to %s (mean=%g, sd=%g)\n", n, out, mean, sd);
    return 0;
}
 