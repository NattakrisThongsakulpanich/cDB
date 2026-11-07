#include <stdio.h>
#include <stdint.h>
#include "../cDB.h"

int main(void) {
    printf("=== Initializing cDB Database ===\n\n");

    // 1. Open Database
    cDB_Database* db = cDB_open("MyEcommerceDB", 10);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    // 2. Define schema & Create table
    // CREATE TABLE users (id INT32, name STRING, age INT32, balance DOUBLE)
    cDB_ColumnDef user_cols[] = {
        {"id",      CDB_TYPE_INT32,  0},
        {"name",    CDB_TYPE_STRING, 64},
        {"age",     CDB_TYPE_INT32,  0},
        {"balance", CDB_TYPE_DOUBLE, 0}
    };

    printf("[1] Creating 'users' table...\n");
    cDB_create_table(db, "users", user_cols, 4, 100);

    // 3. INSERT INTO users VALUES (...)
    printf("[2] Inserting records...\n");
    {
        int32_t id = 1;
        char name[64] = "Alice";
        int32_t age = 28;
        double balance = 1250.50;
        const void* row[] = {&id, name, &age, &balance};
        cDB_insert(db, "users", "user_alice", row);
    }
    {
        int32_t id = 2;
        char name[64] = "Bob";
        int32_t age = 34;
        double balance = 450.00;
        const void* row[] = {&id, name, &age, &balance};
        cDB_insert(db, "users", "user_bob", row);
    }
    {
        int32_t id = 3;
        char name[64] = "Charlie";
        int32_t age = 22;
        double balance = 3200.75;
        const void* row[] = {&id, name, &age, &balance};
        cDB_insert(db, "users", "user_charlie", row);
    }
    {
        int32_t id = 4;
        char name[64] = "Diana";
        int32_t age = 29;
        double balance = 890.10;
        const void* row[] = {&id, name, &age, &balance};
        cDB_insert(db, "users", "user_diana", row);
    }

    // 4. SELECT * FROM users
    printf("\n[3] Query: SELECT * FROM users\n");
    cDB_ResultSet* res_all = cDB_select(db, "users", NULL);
    cDB_print_result(res_all);
    cDB_free_result(res_all);

    // 5. SELECT * FROM users WHERE age > 25
    printf("[4] Query: SELECT * FROM users WHERE age > 25\n");
    int32_t filter_age = 25;
    cDB_Condition cond_age = {
        .col_name = "age",
        .op = CDB_OP_GT,
        .val = &filter_age
    };
    cDB_ResultSet* res_filter = cDB_select(db, "users", &cond_age);
    cDB_print_result(res_filter);
    cDB_free_result(res_filter);

    // 6. UPDATE users SET balance = 1500.00 WHERE name == "Bob"
    printf("[5] Updating: UPDATE users SET balance = 1500.00 WHERE name == 'Bob'\n");
    char target_name[64] = "Bob";
    cDB_Condition cond_name = {
        .col_name = "name",
        .op = CDB_OP_EQ,
        .val = target_name
    };
    double new_balance = 1500.00;
    uint32_t updated = cDB_update(db, "users", "balance", &new_balance, &cond_name);
    printf("Updated %u row(s).\n\n", updated);

    // 7. DELETE FROM users WHERE id == 3 (Charlie)
    printf("[6] Deleting: DELETE FROM users WHERE id == 3\n");
    int32_t delete_id = 3;
    cDB_Condition cond_del = {
        .col_name = "id",
        .op = CDB_OP_EQ,
        .val = &delete_id
    };
    uint32_t deleted = cDB_delete(db, "users", &cond_del);
    printf("Deleted %u row(s).\n\n", deleted);

    // 8. SELECT * FROM users after modifications
    printf("[7] Query: SELECT * FROM users (final state)\n");
    cDB_ResultSet* res_final = cDB_select(db, "users", NULL);
    cDB_print_result(res_final);
    cDB_free_result(res_final);

    // 9. Cleanup
    cDB_close(db);
    printf("Database closed successfully.\n");
    return 0;
}
