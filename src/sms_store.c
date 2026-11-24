#include "sms_store.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(sqlite3 *db, const char *sql)
{
    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

int sms_store_open(sqlite3 **db, const char *path)
{
    int rc = sqlite3_open_v2(path, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_busy_timeout(*db, 3000);
    return run(*db,
        "CREATE TABLE IF NOT EXISTS sms("
        "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,ind TEXT,Mem_Store TEXT,"
        "Tag TEXT,Number TEXT,Cc_Sms TEXT,Cc_Ref TEXT,Cc_Total TEXT,Cc_Seq TEXT,"
        "Cc_Num TEXT,Cc_Content TEXT,Language TEXT,Tp_Dcs TEXT,Msg_Ref TEXT,"
        "Content TEXT,Sms_Report_Recived TEXT,Draft_Group_Id TEXT,Year TEXT,"
        "Month TEXT,Day TEXT,Hour TEXT,Minute TEXT,Second TEXT,TimeZone TEXT,"
        "Mktime INTEGER,DisplayMode TEXT);"
        "CREATE TABLE IF NOT EXISTS ims_rx_group("
        "group_key TEXT PRIMARY KEY, sender TEXT NOT NULL, total INTEGER NOT NULL,"
        "concat_ref INTEGER NOT NULL, year TEXT NOT NULL, month TEXT NOT NULL,"
        "day TEXT NOT NULL, hour TEXT NOT NULL, minute TEXT NOT NULL,"
        "second TEXT NOT NULL, timezone TEXT NOT NULL, epoch INTEGER NOT NULL,"
        "complete INTEGER NOT NULL DEFAULT 0, sms_id INTEGER,"
        "concat_16 INTEGER NOT NULL DEFAULT 0,first_seen INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS ims_rx_part("
        "group_key TEXT NOT NULL, seq INTEGER NOT NULL, body TEXT,fingerprint TEXT NOT NULL,"
        "PRIMARY KEY(group_key,seq));"
        "CREATE TABLE IF NOT EXISTS ims_rx_delivery("
        "tx_key TEXT PRIMARY KEY,group_key TEXT NOT NULL,seq INTEGER NOT NULL,"
        "fingerprint TEXT NOT NULL,received INTEGER NOT NULL,epoch INTEGER NOT NULL);");
}

static void bind_text(sqlite3_stmt *s, int n, const char *value)
{
    sqlite3_bind_text(s, n, value, -1, SQLITE_TRANSIENT);
}

static int prepare(sqlite3 *db, sqlite3_stmt **s, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, s, NULL);
}

static void fingerprint(const char *body, char out[17])
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const unsigned char *s = (const unsigned char *)body;
    while (*s) { hash ^= *s++; hash *= UINT64_C(1099511628211); }
    snprintf(out, 17, "%016llx", (unsigned long long)hash);
}

static int store_part_tx(sqlite3 *db, const struct sms_part *p, int *completed)
{
    sqlite3_stmt *s = NULL;
    int rc, done = 0, count, inserted = 0;
    char hash[17];
    fingerprint(p->utf16be_hex, hash);

    rc = prepare(db, &s,
        "INSERT OR IGNORE INTO ims_rx_group"
        "(group_key,sender,total,concat_ref,year,month,day,hour,minute,second,timezone,epoch)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key); bind_text(s, 2, p->sender);
    sqlite3_bind_int(s, 3, p->total); sqlite3_bind_int(s, 4, p->concat_ref);
    bind_text(s, 5, p->year); bind_text(s, 6, p->month); bind_text(s, 7, p->day);
    bind_text(s, 8, p->hour); bind_text(s, 9, p->minute); bind_text(s, 10, p->second);
    bind_text(s, 11, p->timezone); sqlite3_bind_int64(s, 12, p->epoch);
    rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
    if (rc != SQLITE_DONE) goto fail;

    rc = prepare(db, &s, "SELECT sender,total,concat_ref,complete FROM ims_rx_group WHERE group_key=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key);
    rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) goto fail;
    if (strcmp((const char *)sqlite3_column_text(s, 0), p->sender) ||
        sqlite3_column_int(s, 1) != p->total || sqlite3_column_int(s, 2) != p->concat_ref) {
        rc = SQLITE_CONSTRAINT; goto fail;
    }
    done = sqlite3_column_int(s, 3);
    sqlite3_finalize(s); s = NULL;
    if (!done) {
        rc = prepare(db, &s, "INSERT OR IGNORE INTO ims_rx_part(group_key,seq,body,fingerprint) VALUES(?,?,?,?)");
        if (rc != SQLITE_OK) goto fail;
        bind_text(s, 1, p->group_key); sqlite3_bind_int(s, 2, p->sequence);
        bind_text(s, 3, p->utf16be_hex); bind_text(s, 4, hash);
        rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
        if (rc != SQLITE_DONE) goto fail;
    }

    rc = prepare(db, &s, "SELECT body,fingerprint FROM ims_rx_part WHERE group_key=? AND seq=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key); sqlite3_bind_int(s, 2, p->sequence);
    rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) goto fail;
    if (strcmp((const char *)sqlite3_column_text(s, 1), hash) ||
        (!done && strcmp((const char *)sqlite3_column_text(s, 0), p->utf16be_hex))) {
        rc = SQLITE_CONSTRAINT; goto fail;
    }
    sqlite3_finalize(s); s = NULL;
    if (done) goto finished;

    rc = prepare(db, &s, "SELECT count(*) FROM ims_rx_part WHERE group_key=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key);
    rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) goto fail;
    count = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); s = NULL;
    if (count != p->total) goto finished;

    rc = prepare(db, &s,
        "INSERT INTO sms(ind,Mem_Store,Tag,Number,Cc_Sms,Cc_Ref,Cc_Total,Cc_Seq,"
        "Cc_Num,Cc_Content,Tp_Dcs,Msg_Ref,Content,Sms_Report_Recived,Draft_Group_Id,"
        "Year,Month,Day,Hour,Minute,Second,TimeZone,Mktime,DisplayMode) "
        "SELECT CASE WHEN total=1 THEN '0' ELSE (SELECT group_concat('0',';') FROM "
        "(SELECT seq FROM ims_rx_part WHERE group_key=g.group_key ORDER BY seq)) END,"
        "'nv','1',sender,CASE WHEN total=1 THEN '0' ELSE '1' END,"
        "CASE WHEN total=1 THEN 0 ELSE concat_ref END,CASE WHEN total=1 THEN 0 ELSE total END,"
        "CASE WHEN total=1 THEN '0' ELSE (SELECT group_concat(seq,';') FROM "
        "(SELECT seq FROM ims_rx_part WHERE group_key=g.group_key ORDER BY seq)) END,"
        "CASE WHEN total=1 THEN NULL ELSE total END,"
        "CASE WHEN total=1 THEN NULL ELSE (SELECT group_concat(body,';') FROM "
        "(SELECT body FROM ims_rx_part WHERE group_key=g.group_key ORDER BY seq)) END,"
        "'2','0',(SELECT group_concat(body,'') FROM "
        "(SELECT body FROM ims_rx_part WHERE group_key=g.group_key ORDER BY seq)),"
        "'0','',year,month,day,hour,minute,second,timezone,epoch,'0' "
        "FROM ims_rx_group g WHERE group_key=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key);
    rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
    if (rc != SQLITE_DONE) goto fail;

    rc = prepare(db, &s, "UPDATE ims_rx_group SET complete=1,sms_id=? WHERE group_key=?");
    if (rc != SQLITE_OK) goto fail;
    sqlite3_bind_int64(s, 1, sqlite3_last_insert_rowid(db)); bind_text(s, 2, p->group_key);
    rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
    if (rc != SQLITE_DONE) goto fail;
    rc = prepare(db, &s, "UPDATE ims_rx_part SET body=NULL WHERE group_key=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, p->group_key);
    rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
    if (rc != SQLITE_DONE) goto fail;
    inserted = 1;
finished:
    *completed = inserted;
    return SQLITE_OK;
fail:
    if (s) sqlite3_finalize(s);
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_ERROR : rc;
}

int sms_store_part(sqlite3 *db, const struct sms_part *p, int *completed)
{
    int rc;
    *completed = 0;
    if (p->total < 1 || p->total > 255 || p->sequence < 1 || p->sequence > p->total)
        return SQLITE_MISUSE;
    rc = run(db, "BEGIN IMMEDIATE");
    if (rc == SQLITE_OK) {
        rc = store_part_tx(db, p, completed);
        if (rc == SQLITE_OK) rc = run(db, "COMMIT");
        if (rc != SQLITE_OK) { run(db, "ROLLBACK"); *completed = 0; }
    }
    return rc;
}

static int prune_completed(sqlite3 *db, sqlite3_int64 cutoff)
{
    static const char *sql[] = {
        "DELETE FROM ims_rx_delivery WHERE group_key IN "
        "(SELECT group_key FROM ims_rx_group WHERE complete=1 AND first_seen>0 AND first_seen<?)",
        "DELETE FROM ims_rx_part WHERE group_key IN "
        "(SELECT group_key FROM ims_rx_group WHERE complete=1 AND first_seen>0 AND first_seen<?)",
        "DELETE FROM ims_rx_group WHERE complete=1 AND first_seen>0 AND first_seen<?"
    };
    sqlite3_stmt *s;
    size_t i;
    int rc;
    for (i = 0; i < sizeof(sql) / sizeof(sql[0]); i++) {
        rc = prepare(db, &s, sql[i]);
        if (rc != SQLITE_OK) return rc;
        sqlite3_bind_int64(s, 1, cutoff);
        rc = sqlite3_step(s);
        sqlite3_finalize(s);
        if (rc != SQLITE_DONE) return rc;
    }
    return SQLITE_OK;
}

int sms_store_receive(sqlite3 *db, const struct sms_part *part, int concat_16bit,
                      const char *transaction_key, sqlite3_int64 now, int *completed)
{
    sqlite3_stmt *s = NULL;
    struct sms_part selected = *part;
    char hash[17], *group = NULL;
    int rc, candidates = 0, is_new = 0;
    *completed = 0;
    if (!transaction_key || !*transaction_key || now <= 0 || part->total < 1 ||
        part->total > 255 || part->sequence < 1 || part->sequence > part->total)
        return SQLITE_MISUSE;
    fingerprint(part->utf16be_hex, hash);
    rc = run(db, "BEGIN IMMEDIATE");
    if (rc != SQLITE_OK) return rc;
    rc = prepare(db, &s,
        "SELECT d.group_key,d.seq,d.fingerprint,g.sender,g.total,g.concat_ref,g.concat_16,d.epoch "
        "FROM ims_rx_delivery d JOIN ims_rx_group g ON g.group_key=d.group_key WHERE d.tx_key=?");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, transaction_key);
    rc = sqlite3_step(s);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_int(s, 1) != part->sequence ||
            strcmp((const char *)sqlite3_column_text(s, 2), hash) ||
            strcmp((const char *)sqlite3_column_text(s, 3), part->sender) ||
            sqlite3_column_int(s, 4) != part->total ||
            sqlite3_column_int(s, 5) != part->concat_ref ||
            sqlite3_column_int(s, 6) != !!concat_16bit ||
            sqlite3_column_int64(s, 7) != part->epoch) { rc = SQLITE_CONSTRAINT; goto fail; }
        sqlite3_finalize(s); s = NULL;
        rc = run(db, "COMMIT");
        if (rc != SQLITE_OK) goto fail;
        return SQLITE_OK;
    }
    if (rc != SQLITE_DONE) goto fail;
    sqlite3_finalize(s); s = NULL;
    if (part->total > 1) {
        rc = prepare(db, &s,
            "SELECT g.group_key,g.complete,p.fingerprint FROM ims_rx_group g "
            "LEFT JOIN ims_rx_part p ON p.group_key=g.group_key AND p.seq=? "
            "WHERE g.sender=? AND g.total=? AND g.concat_ref=? AND g.concat_16=? "
            "AND g.epoch BETWEEN ? AND ? AND g.first_seen BETWEEN ? AND ?");
        if (rc != SQLITE_OK) goto fail;
        sqlite3_bind_int(s, 1, part->sequence); bind_text(s, 2, part->sender);
        sqlite3_bind_int(s, 3, part->total); sqlite3_bind_int(s, 4, part->concat_ref);
        sqlite3_bind_int(s, 5, !!concat_16bit);
        sqlite3_bind_int64(s, 6, part->epoch - 600); sqlite3_bind_int64(s, 7, part->epoch + 600);
        sqlite3_bind_int64(s, 8, now - 604800); sqlite3_bind_int64(s, 9, now + 600);
        while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
            const unsigned char *existing = sqlite3_column_text(s, 2);
            int complete = sqlite3_column_int(s, 1);
            if ((existing && strcmp((const char *)existing, hash)) || (!existing && complete)) continue;
            candidates++;
            if (candidates == 1) group = sqlite3_mprintf("%s", sqlite3_column_text(s, 0));
        }
        if (rc != SQLITE_DONE) goto fail;
        sqlite3_finalize(s); s = NULL;
        if (candidates > 1) { rc = SQLITE_CONSTRAINT; goto fail; }
        if (candidates == 1 && !group) { rc = SQLITE_NOMEM; goto fail; }
    }
    if (!group) {
        group = sqlite3_mprintf("%s", transaction_key);
        if (!group) { rc = SQLITE_NOMEM; goto fail; }
        is_new = 1;
    }
    selected.group_key = group;
    rc = store_part_tx(db, &selected, completed);
    if (rc != SQLITE_OK) goto fail;
    if (is_new) {
        rc = prepare(db, &s, "UPDATE ims_rx_group SET concat_16=?,first_seen=? WHERE group_key=?");
        if (rc != SQLITE_OK) goto fail;
        sqlite3_bind_int(s, 1, !!concat_16bit); sqlite3_bind_int64(s, 2, now); bind_text(s, 3, group);
        rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
        if (rc != SQLITE_DONE) goto fail;
    }
    rc = prepare(db, &s,
        "INSERT INTO ims_rx_delivery(tx_key,group_key,seq,fingerprint,received,epoch) VALUES(?,?,?,?,?,?)");
    if (rc != SQLITE_OK) goto fail;
    bind_text(s, 1, transaction_key); bind_text(s, 2, group);
    sqlite3_bind_int(s, 3, part->sequence); bind_text(s, 4, hash); sqlite3_bind_int64(s, 5, now);
    sqlite3_bind_int64(s, 6, part->epoch);
    rc = sqlite3_step(s); sqlite3_finalize(s); s = NULL;
    if (rc != SQLITE_DONE) goto fail;
    rc = prune_completed(db, now - 604800);
    if (rc != SQLITE_OK) goto fail;
    rc = run(db, "COMMIT");
    if (rc != SQLITE_OK) goto fail;
    sqlite3_free(group);
    return SQLITE_OK;
fail:
    if (s) sqlite3_finalize(s);
    run(db, "ROLLBACK");
    sqlite3_free(group);
    *completed = 0;
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_ERROR : rc;
}

int sms_store_encode(const char *utf8, char **hex)
{
    static const char digits[] = "0123456789ABCDEF";
    const unsigned char *p = (const unsigned char *)utf8;
    char *out = malloc(strlen(utf8) * 4 + 1), *q = out;
    if (!out) return SQLITE_NOMEM;
    while (*p) {
        unsigned int cp, min, n;
        if (*p < 0x80) { cp = *p++; n = 0; min = 0; }
        else if ((*p & 0xe0) == 0xc0) { cp = *p++ & 0x1f; n = 1; min = 0x80; }
        else if ((*p & 0xf0) == 0xe0) { cp = *p++ & 0x0f; n = 2; min = 0x800; }
        else if ((*p & 0xf8) == 0xf0) { cp = *p++ & 7; n = 3; min = 0x10000; }
        else goto invalid;
        while (n--) {
            if ((*p & 0xc0) != 0x80) goto invalid;
            cp = (cp << 6) | (*p++ & 0x3f);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) goto invalid;
        if (cp >= 0x10000) {
            unsigned int high = 0xd800 + ((cp - 0x10000) >> 10);
            *q++ = digits[high >> 12]; *q++ = digits[(high >> 8) & 15];
            *q++ = digits[(high >> 4) & 15]; *q++ = digits[high & 15];
            cp = 0xdc00 + ((cp - 0x10000) & 0x3ff);
        }
        *q++ = digits[cp >> 12]; *q++ = digits[(cp >> 8) & 15];
        *q++ = digits[(cp >> 4) & 15]; *q++ = digits[cp & 15];
    }
    *q = 0;
    *hex = out;
    return SQLITE_OK;
invalid:
    free(out);
    return SQLITE_MISMATCH;
}
