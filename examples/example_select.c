#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "cDB.h"

// Context passed into custom predicates
typedef struct {
    cDB_Attribute age_attr;
    int min_age;
} AgeFilterCtx;

typedef struct {
    cDB_Attribute dept_attr;
    const char* target_dept;
} DeptFilterCtx;

typedef struct {
    cDB_Attribute dept_attr;
    cDB_Attribute salary_attr;
    const char* target_dept;
    int min_salary;
} CompoundFilterCtx;

// Predicate 1: Filter by minimum age
static bool filter_by_age(cDB_Database* db, cDB_Record r, void* user_data)
{
    AgeFilterCtx* ctx = (AgeFilterCtx*)user_data;
    char* age_str = (char*)cDB_getRecordValue(db, r, ctx->age_attr);
    if (!age_str) return false;
    return atoi(age_str) >= ctx->min_age;
}

// Predicate 2: Filter by department string
static bool filter_by_dept(cDB_Database* db, cDB_Record r, void* user_data)
{
    DeptFilterCtx* ctx = (DeptFilterCtx*)user_data;
    char* dept = (char*)cDB_getRecordValue(db, r, ctx->dept_attr);
    if (!dept) return false;
    return strcmp(dept, ctx->target_dept) == 0;
}

// Predicate 3: Compound filter (Dept == target AND Salary >= min_salary)
static bool filter_compound(cDB_Database* db, cDB_Record r, void* user_data)
{
    CompoundFilterCtx* ctx = (CompoundFilterCtx*)user_data;
    char* dept = (char*)cDB_getRecordValue(db, r, ctx->dept_attr);
    char* salary_str = (char*)cDB_getRecordValue(db, r, ctx->salary_attr);

    if (!dept || !salary_str) return false;
    return (strcmp(dept, ctx->target_dept) == 0) && (atoi(salary_str) >= ctx->min_salary);
}

int main(void)
{
    printf("=== Example 2: Conditional and Compound Selection ===\n\n");

    cDB_Database* db = cDB_Init();
    cDB_newTable(db, "employees", 10, 4);

    cDB_Attribute name_attr   = cDB_createAttribute(db, "Name",   CDB_STRING, 32);
    cDB_Attribute dept_attr   = cDB_createAttribute(db, "Dept",   CDB_STRING, 16);
    cDB_Attribute age_attr    = cDB_createAttribute(db, "Age",    CDB_STRING, 8);
    cDB_Attribute salary_attr = cDB_createAttribute(db, "Salary", CDB_STRING, 16);

    // Populate employees table
    struct { const char* alias; const char* name; const char* dept; const char* age; const char* salary; } staff[] = {
        { "EMP_1", "Alice Jenkins",  "Engineering", "28", "85000" },
        { "EMP_2", "Bob Smith",      "Marketing",   "34", "62000" },
        { "EMP_3", "Charlie Davis",  "Engineering", "42", "115000" },
        { "EMP_4", "Diana Prince",   "Human Res",   "25", "54000" },
        { "EMP_5", "Evan Wright",    "Engineering", "23", "72000" },
        { "EMP_6", "Fiona Gallagher", "Marketing",  "38", "94000" }
    };

    for (size_t i = 0; i < sizeof(staff)/sizeof(staff[0]); i++) {
        cDB_setRecordAlias(db, i, staff[i].alias);
        cDB_setRecordValue(db, i, name_attr, (void*)staff[i].name);
        cDB_setRecordValue(db, i, dept_attr, (void*)staff[i].dept);
        cDB_setRecordValue(db, i, age_attr, (void*)staff[i].age);
        cDB_setRecordValue(db, i, salary_attr, (void*)staff[i].salary);
    }

    // Query 1: All employees with Age >= 30
    printf("--- Query 1: Age >= 30 ---\n");
    AgeFilterCtx age_filter = { .age_attr = age_attr, .min_age = 30 };
    cDB_select(db, filter_by_age, &age_filter);
    printf("Matched %llu record(s).\n", (unsigned long long)cDB_resultLen(db));

    cDB_Projection* proj1 = cDB_getResultProjection(db);
    cDB_printProjection(db, proj1, false, true, NULL, NULL, NULL);
    cDB_freeProjection(proj1);

    // Query 2: Employees in "Marketing"
    printf("--- Query 2: Dept == 'Marketing' ---\n");
    DeptFilterCtx dept_filter = { .dept_attr = dept_attr, .target_dept = "Marketing" };
    cDB_select(db, filter_by_dept, &dept_filter);
    printf("Matched %llu record(s).\n", (unsigned long long)cDB_resultLen(db));

    cDB_Projection* proj2 = cDB_getResultProjection(db);
    cDB_printProjection(db, proj2, false, true, NULL, NULL, NULL);
    cDB_freeProjection(proj2);

    // Query 3: Compound (Dept == "Engineering" AND Salary >= 80000)
    printf("--- Query 3: Dept == 'Engineering' AND Salary >= 80000 ---\n");
    CompoundFilterCtx compound = {
        .dept_attr = dept_attr,
        .salary_attr = salary_attr,
        .target_dept = "Engineering",
        .min_salary = 80000
    };
    cDB_select(db, filter_compound, &compound);
    printf("Matched %llu record(s).\n", (unsigned long long)cDB_resultLen(db));

    cDB_Projection* proj3 = cDB_getResultProjection(db);
    cDB_printProjection(db, proj3, true, true, NULL, NULL, NULL);
    cDB_freeProjection(proj3);

    cDB_Close(db);
    return 0;
}
