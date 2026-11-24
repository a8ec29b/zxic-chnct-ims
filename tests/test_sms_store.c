#define _XOPEN_SOURCE 700
#include "sms_store.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void check_text(sqlite3 *db, const char *sql, const char *expected)
{
    sqlite3_stmt *s = NULL;
    assert(sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW);
    assert(strcmp((const char *)sqlite3_column_text(s, 0), expected) == 0);
    sqlite3_finalize(s);
}

int main(void)
{
    char path[] = "/tmp/zxic-sms-test-XXXXXX";
    int fd = mkstemp(path), completed = 0;
    sqlite3 *db = NULL;
    char *hex = NULL;
    struct sms_part p = {
        "single", "+861234", "4E2D6587", 1, 1, 0,
        "26", "11", "24", "08", "00", "01", "+32", 1795478401
    };
    assert(fd >= 0); close(fd);
    assert(sms_store_open(&db, path) == SQLITE_OK);
    assert(sms_store_encode("中A😀", &hex) == SQLITE_OK);
    assert(strcmp(hex, "4E2D0041D83DDE00") == 0); free(hex);
    assert(sms_store_encode("\xc0\x80", &hex) == SQLITE_MISMATCH);

    assert(sms_store_part(db, &p, &completed) == SQLITE_OK && completed == 1);
    assert(sms_store_part(db, &p, &completed) == SQLITE_OK && completed == 0);
    check_text(db, "SELECT Mem_Store||'/'||Tag||'/'||Number||'/'||Content FROM sms", "nv/1/+861234/4E2D6587");
    check_text(db, "SELECT count(*) FROM sms", "1");

    p.group_key = "multipart"; p.total = 2; p.concat_ref = 42;
    p.sequence = 2; p.utf16be_hex = "6587";
    assert(sms_store_part(db, &p, &completed) == SQLITE_OK && completed == 0);
    p.utf16be_hex = "0041";
    assert(sms_store_part(db, &p, &completed) == SQLITE_CONSTRAINT);
    p.sequence = 1; p.utf16be_hex = "4E2D";
    assert(sms_store_part(db, &p, &completed) == SQLITE_OK && completed == 1);
    check_text(db, "SELECT ind||'/'||Cc_Sms||'/'||Cc_Ref||'/'||Cc_Total||'/'||Cc_Seq||'/'||Cc_Num||'/'||Cc_Content||'/'||Content FROM sms WHERE id=2", "0;0/1/42/2/1;2/2/4E2D;6587/4E2D6587");
    check_text(db, "SELECT Tag FROM sms WHERE id=2", "1");
    check_text(db, "SELECT count(*) FROM ims_rx_part WHERE body IS NOT NULL", "0");
    p.sequence = 2; p.utf16be_hex = "6587";
    assert(sms_store_part(db, &p, &completed) == SQLITE_OK && completed == 0);
    check_text(db, "SELECT count(*) FROM sms", "2");

    p.group_key = NULL; p.concat_ref = 17; p.total = 2; p.sequence = 2;
    p.utf16be_hex = "6587";
    assert(sms_store_receive(db, &p, 0, "sms-b", 1795478420, &completed) == SQLITE_OK && !completed);
    assert(sms_store_receive(db, &p, 0, "sms-b", 1795478421, &completed) == SQLITE_OK && !completed);
    sqlite3_close(db);
    assert(sms_store_open(&db, path) == SQLITE_OK);
    p.utf16be_hex = "0041";
    assert(sms_store_receive(db, &p, 0, "sms-b", 1795478421, &completed) == SQLITE_CONSTRAINT);
    p.sequence = 1; p.utf16be_hex = "4E2D";
    assert(sms_store_receive(db, &p, 0, "sms-a", 1795478422, &completed) == SQLITE_OK && completed);
    check_text(db, "SELECT Content FROM sms WHERE id=3", "4E2D6587");
    p.sequence = 2; p.utf16be_hex = "6587";
    assert(sms_store_receive(db, &p, 0, "sms-b-retry", 1795478423, &completed) == SQLITE_OK && !completed);
    p.utf16be_hex = "0041";
    assert(sms_store_receive(db, &p, 0, "new-message-b", 1795478424, &completed) == SQLITE_OK && !completed);
    p.sequence = 1; p.utf16be_hex = "0042";
    assert(sms_store_receive(db, &p, 0, "new-message-a", 1795478425, &completed) == SQLITE_OK && completed);
    check_text(db, "SELECT Content FROM sms WHERE id=4", "00420041");
    p.group_key = "multipart"; p.utf16be_hex = "FFFF";
    assert(sms_store_part(db, &p, &completed) == SQLITE_CONSTRAINT && !completed);

    p.group_key = NULL; p.concat_ref = 33; p.sequence = 1; p.utf16be_hex = "0043";
    assert(sms_store_receive(db, &p, 0, "unfinished", 1795478430, &completed) == SQLITE_OK && !completed);
    p.total = 1; p.sequence = 1; p.concat_ref = 0; p.utf16be_hex = "0044";
    assert(sms_store_receive(db, &p, 0, "later", 1796083231, &completed) == SQLITE_OK && completed);
    check_text(db, "SELECT count(*) FROM ims_rx_group WHERE group_key='unfinished'", "1");
    check_text(db, "SELECT count(*) FROM ims_rx_group WHERE group_key='sms-b'", "0");
    check_text(db, "SELECT count(*) FROM sms", "5");
    sqlite3_close(db);
    unlink(path);
    puts("sms_store: ok");
    return 0;
}
