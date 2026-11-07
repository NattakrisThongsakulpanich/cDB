#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_NAME_LENGTH 33
#define MAX_ID_LENGTH 12
#define MAX_RECORDS 128
#define MIN_SCORE 0
#define MAX_SCORE 100
#define GRADES "ABCDF"

typedef struct
{
    char ID[MAX_ID_LENGTH];
    char name[MAX_NAME_LENGTH];
    int score;
} StudentRecord;

int valid_score(int s)
{
    return s >= MIN_SCORE && s <= MAX_SCORE;
}

void skip_line(void)
{
    int ch = fgetc(stdin);
    while (ch != '\n' && ch != EOF)
    {
        ch = fgetc(stdin);
    }
}

int load(const char *fileName, StudentRecord r[], int max, int *complete)
{
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        perror(fileName);
        return -1;
    }

    int n = 0;
    int result = 3;
    *complete = 1;

    while (n < max && (result = fscanf(fp, "%11s %32s %d", r[n].ID, r[n].name, &r[n].score)) == 3)
    {
        if (!valid_score(r[n].score))
        {
            result = 0;
            break;
        }
        n++;
    }

    if (n == max)
    {
        printf("Warning: only the first %d records were read.\n", max);
        *complete = 0;
    }
    else if (result != EOF)
    {
        printf("Warning: record %d of \"%s\" is invalid, reading stopped there.\n", n + 1, fileName);
        *complete = 0;
    }

    fclose(fp);
    return n;
}

int save(const char *fileName, const StudentRecord r[], int n, int complete)
{
    if (!complete)
    {
        printf("Not saved: the file has records that were not read and would be lost.\n");
        return 0;
    }

    FILE *fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        perror(fileName);
        return 0;
    }

    for (int i = 0; i < n; i++)
    {
        fprintf(fp, "%s %s %d\n", r[i].ID, r[i].name, r[i].score);
    }

    fclose(fp);
    return 1;
}

int find_index(const StudentRecord r[], int n, const char *id)
{
    for (int i = 0; i < n; i++)
    {
        if (strcmp(r[i].ID, id) == 0)
        {
            return i;
        }
    }
    return -1;
}

void calc_stats(const StudentRecord r[], int n, double *mean, double *sd)
{
    double sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += r[i].score;
    }
    *mean = sum / n;

    double sq = 0;
    for (int i = 0; i < n; i++)
    {
        sq += (r[i].score - *mean) * (r[i].score - *mean);
    }
    *sd = sqrt(sq / n);
}

double t_score(int score, double mean, double sd)
{
    if (sd == 0)
    {
        return 50.0;
    }
    return 50.0 + 10.0 * (score - mean) / sd;
}

int grade_from(double v, const double cut[])
{
    for (int i = 0; i < 4; i++)
    {
        if (v >= cut[i])
        {
            return i;
        }
    }
    return 4;
}

int raw_grade(int score)
{
    const double cut[4] = {80, 70, 60, 50};
    return grade_from(score, cut);
}

int t_grade(double t)
{
    const double cut[4] = {60, 55, 45, 40};
    return grade_from(t, cut);
}

void sort_records(StudentRecord r[], int n, int mode)
{
    if (mode != 1 && mode != 2)
    {
        return;
    }

    for (int i = 0; i < n - 1; i++)
    {
        int best = i;
        for (int j = i + 1; j < n; j++)
        {
            int better;
            if (mode == 1)
            {
                better = strcmp(r[j].ID, r[best].ID) < 0;
            }
            else
            {
                better = r[j].score > r[best].score;
            }

            if (better)
            {
                best = j;
            }
        }

        StudentRecord temp = r[i];
        r[i] = r[best];
        r[best] = temp;
    }
}

int ask_sort_mode(void)
{
    int mode;
    printf("Sort by (0 = none, 1 = ID, 2 = score): ");
    if (scanf("%d", &mode) != 1)
    {
        skip_line();
        return 0;
    }
    return mode;
}

void print_line(int grouped)
{
    printf("+-------------+----------------------------------+----------+");
    if (grouped)
    {
        printf("---------+-------+");
    }
    printf("\n");
}

void show_table(const char *fileName, int grouped)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = load(fileName, r, MAX_RECORDS, &complete);
    if (n < 0)
    {
        return;
    }
    if (grouped && n < 2)
    {
        printf("Need at least 2 records for group-based grading.\n");
        return;
    }

    sort_records(r, n, ask_sort_mode());

    double mean = 0, sd = 0;
    if (grouped)
    {
        calc_stats(r, n, &mean, &sd);
    }

    printf("%s\n", fileName);
    print_line(grouped);
    printf("| ID          | Name                             | Score    |");
    if (grouped)
    {
        printf(" T-score | Grade |");
    }
    printf("\n");
    print_line(grouped);

    int gradeCount[5] = {0};
    for (int i = 0; i < n; i++)
    {
        printf("| %-11s | %-32s | %-8d |", r[i].ID, r[i].name, r[i].score);
        if (grouped)
        {
            double t = t_score(r[i].score, mean, sd);
            int g = t_grade(t);
            gradeCount[g]++;
            printf(" %7.2f | %-5c |", t, GRADES[g]);
        }
        printf("\n");
    }
    print_line(grouped);

    if (grouped)
    {
        printf("Students: %d   Mean: %.2f   SD: %.2f\n", n, mean, sd);
        printf("Grades:");
        for (int k = 0; k < 5; k++)
        {
            printf("  %c=%d", GRADES[k], gradeCount[k]);
        }
        printf("\n");
        if (sd == 0)
        {
            printf("Everyone has the same score!\n");
        }
    }
}

void query_mode(const char *fileName)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = load(fileName, r, MAX_RECORDS, &complete);
    if (n < 0)
    {
        return;
    }

    double mean = 0, sd = 0;
    if (n >= 2)
    {
        calc_stats(r, n, &mean, &sd);
    }

    printf("Type an ID or a name to view the score and grade. Type \"exit\" to go back.\n");

    char key[MAX_NAME_LENGTH];
    printf(">>> ");
    while (scanf("%32s", key) == 1 && strcmp(key, "exit") != 0)
    {
        int found = 0;
        for (int i = 0; i < n; i++)
        {
            if (strcmp(r[i].ID, key) == 0 || strcmp(r[i].name, key) == 0)
            {
                found++;
                printf("ID: %s\nName: %s\nScore: %d\nGrade: %c\n",
                       r[i].ID, r[i].name, r[i].score, GRADES[raw_grade(r[i].score)]);

                if (n >= 2)
                {
                    double t = t_score(r[i].score, mean, sd);
                    printf("T-score: %.2f\nGroup grade: %c\n", t, GRADES[t_grade(t)]);
                }
                printf("\n");
            }
        }
        if (found == 0)
        {
            printf("Unable to find: %s\n", key);
        }
        printf(">>> ");
    }
}

int needs_newline(const char *fileName)
{
    FILE *fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        return 0;
    }

    int need = 0;
    if (fseek(fp, -1, SEEK_END) == 0)
    {
        need = (fgetc(fp) != '\n');
    }

    fclose(fp);
    return need;
}

void append_mode(const char *fileName)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = 0;

    FILE *test = fopen(fileName, "r");
    if (test != NULL)
    {
        fclose(test);
        n = load(fileName, r, MAX_RECORDS, &complete);
        if (n < 0)
        {
            return;
        }
    }

    int addNewline = needs_newline(fileName);
    FILE *fp = fopen(fileName, "a");
    if (fp == NULL)
    {
        perror(fileName);
        return;
    }
    if (addNewline)
    {
        fputc('\n', fp);
    }

    printf("Type a student as \"ID NAME SCORE\" (score %d-%d). Type \"exit\" to go back.\n",
           MIN_SCORE, MAX_SCORE);

    StudentRecord rec;
    printf(">>> ");
    while (scanf("%11s", rec.ID) == 1 && strcmp(rec.ID, "exit") != 0)
    {
        if (scanf("%32s %d", rec.name, &rec.score) != 2)
        {
            printf("Invalid input! Use: ID NAME SCORE\n");
            skip_line();
        }
        else if (!valid_score(rec.score))
        {
            printf("Score must be %d-%d.\n", MIN_SCORE, MAX_SCORE);
        }
        else if (find_index(r, n, rec.ID) >= 0)
        {
            printf("ID %s already exists.\n", rec.ID);
        }
        else if (n >= MAX_RECORDS)
        {
            printf("Database is full (%d records).\n", MAX_RECORDS);
        }
        else
        {
            r[n] = rec;
            n++;
            fprintf(fp, "%s %s %d\n", rec.ID, rec.name, rec.score);
        }
        printf(">>> ");
    }

    fclose(fp);
}

void delete_mode(const char *fileName)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = load(fileName, r, MAX_RECORDS, &complete);
    if (n < 0)
    {
        return;
    }

    char id[MAX_ID_LENGTH];
    printf("ID to delete: ");
    if (scanf("%11s", id) != 1)
    {
        return;
    }

    int pos = find_index(r, n, id);
    if (pos < 0)
    {
        printf("Unable to find ID: %s\n", id);
        return;
    }

    StudentRecord gone = r[pos];
    for (int i = pos; i < n - 1; i++)
    {
        r[i] = r[i + 1];
    }
    n--;

    if (save(fileName, r, n, complete))
    {
        printf("Deleted: %s %s %d\n", gone.ID, gone.name, gone.score);
    }
}

void edit_mode(const char *fileName)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = load(fileName, r, MAX_RECORDS, &complete);
    if (n < 0)
    {
        return;
    }

    char id[MAX_ID_LENGTH];
    printf("ID to edit: ");
    if (scanf("%11s", id) != 1)
    {
        return;
    }

    int pos = find_index(r, n, id);
    if (pos < 0)
    {
        printf("Unable to find ID: %s\n", id);
        return;
    }

    int newScore;
    printf("New score for %s (%d-%d): ", r[pos].name, MIN_SCORE, MAX_SCORE);
    if (scanf("%d", &newScore) != 1)
    {
        skip_line();
        printf("Score must be a whole number %d-%d.\n", MIN_SCORE, MAX_SCORE);
        return;
    }
    if (!valid_score(newScore))
    {
        printf("Score must be %d-%d.\n", MIN_SCORE, MAX_SCORE);
        return;
    }

    int old = r[pos].score;
    r[pos].score = newScore;
    if (save(fileName, r, n, complete))
    {
        printf("Updated %s %s: %d -> %d\n", r[pos].ID, r[pos].name, old, newScore);
    }
}

void stats_mode(const char *fileName)
{
    StudentRecord r[MAX_RECORDS];
    int complete;
    int n = load(fileName, r, MAX_RECORDS, &complete);
    if (n < 0)
    {
        return;
    }
    if (n == 0)
    {
        printf("No records.\n");
        return;
    }

    double mean, sd;
    calc_stats(r, n, &mean, &sd);

    int gradeCount[5] = {0};
    for (int i = 0; i < n; i++)
    {
        gradeCount[raw_grade(r[i].score)]++;
    }

    sort_records(r, n, 2);
    double median;
    if (n % 2 == 1)
    {
        median = r[n / 2].score;
    }
    else
    {
        median = (r[n / 2 - 1].score + r[n / 2].score) / 2.0;
    }

    printf("%s (statistics)\n", fileName);
    printf("Students : %d\n", n);
    printf("Mean     : %.2f\n", mean);
    printf("Median   : %.2f\n", median);
    printf("SD       : %.2f\n", sd);
    printf("Highest  : %d (%s %s)\n", r[0].score, r[0].ID, r[0].name);
    printf("Lowest   : %d (%s %s)\n", r[n - 1].score, r[n - 1].ID, r[n - 1].name);
    printf("Grades (by raw score):");
    for (int k = 0; k < 5; k++)
    {
        printf("  %c=%d", GRADES[k], gradeCount[k]);
    }
    printf("\n");
}

int main(void)
{
    char fileName[100];
    printf("Database file name: ");
    if (scanf("%99s", fileName) != 1)
    {
        return 1;
    }

    int choice = -1;
    do
    {
        printf("\n===== Student Database (%s) =====\n", fileName);
        printf(" 1) View records\n");
        printf(" 2) View with group-based grading\n");
        printf(" 3) Query by ID or name\n");
        printf(" 4) Statistics\n");
        printf(" 5) Append records\n");
        printf(" 6) Delete a record\n");
        printf(" 7) Edit a score\n");
        printf(" 0) Exit\n");
        printf("Choose: ");

        int got = scanf("%d", &choice);
        if (got == EOF)
        {
            choice = 0;
        }
        else if (got != 1)
        {
            skip_line();
            choice = -1;
        }

        switch (choice)
        {
            case 1: show_table(fileName, 0); break;
            case 2: show_table(fileName, 1); break;
            case 3: query_mode(fileName);    break;
            case 4: stats_mode(fileName);    break;
            case 5: append_mode(fileName);   break;
            case 6: delete_mode(fileName);   break;
            case 7: edit_mode(fileName);     break;
            case 0: break;
            default: printf("Invalid choice.\n");
        }
    } while (choice != 0);

    return 0;
}