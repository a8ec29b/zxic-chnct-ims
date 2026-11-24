#include "sms_receipt.h"
#include "sms_store.h"
#include "sms_incoming.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const unsigned char rp[] = {
        0x01,0x34,0x00,0x00,0x15,0x00,0x05,0x91,0x21,0x43,0xf5,0x00,0x00,
        0x62,0x11,0x42,0x80,0x00,0x10,0x23,0x05,0xe8,0x32,0x9b,0xfd,0x06
    };
    const unsigned char ack[] = {2,0x34,0x41,2,0,0};
    sqlite3 *db;
    sqlite3_stmt *query;
    unsigned char report[9];
    size_t length;
    int completed;
    char packet[2048];
    struct sip_message parsed;
    struct sms_incoming_result incoming;
    struct sms_report_request config = {
        .identity="sip:user@example.com", .local_address="2001:db8::1", .call_id="outgoing",
        .tag="local", .branch="z9hG4bKout", .local_port=4001, .cseq=1
    };
    int header_length;
    assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
    assert(sqlite3_exec(db,
        "CREATE TABLE sms(id INTEGER PRIMARY KEY,ind TEXT,Mem_Store TEXT,Tag TEXT,Number TEXT,"
        "Cc_Sms TEXT,Cc_Ref TEXT,Cc_Total TEXT,Cc_Seq TEXT,Cc_Num TEXT,Cc_Content TEXT,"
        "Tp_Dcs TEXT,Msg_Ref TEXT,Content TEXT,Sms_Report_Recived TEXT,Draft_Group_Id TEXT,"
        "Year TEXT,Month TEXT,Day TEXT,Hour TEXT,Minute TEXT,Second TEXT,TimeZone TEXT,"
        "Mktime INTEGER,DisplayMode TEXT)", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db,
        "CREATE TABLE ims_rx_group(group_key TEXT PRIMARY KEY,sender TEXT NOT NULL,total INTEGER NOT NULL,"
        "concat_ref INTEGER NOT NULL,year TEXT NOT NULL,month TEXT NOT NULL,day TEXT NOT NULL,"
        "hour TEXT NOT NULL,minute TEXT NOT NULL,second TEXT NOT NULL,timezone TEXT NOT NULL,"
        "epoch INTEGER NOT NULL,complete INTEGER NOT NULL DEFAULT 0,sms_id INTEGER,"
        "concat_16 INTEGER NOT NULL DEFAULT 0,first_seen INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE ims_rx_part(group_key TEXT NOT NULL,seq INTEGER NOT NULL,body TEXT,fingerprint TEXT NOT NULL,"
        "PRIMARY KEY(group_key,seq));CREATE TABLE ims_rx_delivery(tx_key TEXT PRIMARY KEY,"
        "group_key TEXT NOT NULL,seq INTEGER NOT NULL,fingerprint TEXT NOT NULL,received INTEGER NOT NULL,"
        "epoch INTEGER NOT NULL)", NULL, NULL, NULL) == SQLITE_OK);
    assert(sms_receipt(db, "single", 1795478410, rp, sizeof(rp), report, &length, &completed) == 0);
    assert(length == sizeof(ack) && !memcmp(report, ack, length) && completed == 1);
    assert(sqlite3_prepare_v2(db, "SELECT Content FROM sms", -1, &query, NULL) == SQLITE_OK);
    assert(sqlite3_step(query) == SQLITE_ROW &&
           !strcmp((const char *)sqlite3_column_text(query, 0), "00680065006C006C006F"));
    sqlite3_finalize(query);
    header_length = snprintf(packet, sizeof(packet),
        "MESSAGE sip:user@example.com SIP/2.0\r\nVia: SIP/2.0/UDP [2001:db8::2]:5000;branch=z9hG4bKin\r\n"
        "From: <sip:smsc@example.com>;tag=remote\r\nTo: <sip:user@example.com>\r\n"
        "Call-ID: inbound\r\nCSeq: 7 MESSAGE\r\nP-Asserted-Identity: <sip:smsc@example.com>\r\n"
        "Content-Type: application/vnd.3gpp.sms\r\nContent-Length: %zu\r\n\r\n", sizeof(rp));
    assert(header_length > 0 && (size_t)header_length + sizeof(rp) < sizeof(packet));
    memcpy(packet + header_length, rp, sizeof(rp));
    assert(sms_incoming_process(db, packet, (size_t)header_length + sizeof(rp), 1795478410,
                                "response-tag", &config, &incoming) == 0 && incoming.completed);
    assert(sip_parse(incoming.sip_response, incoming.sip_response_length, &parsed) == 0);
    assert(parsed.start_line.length == 14 && !memcmp(parsed.start_line.data, "SIP/2.0 200 OK", 14));
    assert(sip_parse(incoming.report_message, incoming.report_message_length, &parsed) == 0);
    assert(parsed.body.length == sizeof(ack) && !memcmp(parsed.body.data, ack, sizeof(ack)));
    assert(sms_incoming_process(db, packet, (size_t)header_length + sizeof(rp), 1795478411,
                                "response-tag", &config, &incoming) == 0 && !incoming.completed);
    assert(sqlite3_exec(db, "DROP TABLE sms", NULL, NULL, NULL) == SQLITE_OK);
    assert(sms_receipt(db, "other", 1795478410, rp, sizeof(rp), report, &length, &completed) == 0);
    assert(length == 9 && report[0] == 4 && report[3] == 111 && !completed);
    assert(sms_receipt(db, "bad", 1795478410, rp, sizeof(rp)-1, report, &length, &completed) == 0);
    assert(length == 4 && report[0] == 4 && report[3] == 96);
    sqlite3_close(db);
    puts("sms_receipt: ok");
    return 0;
}
