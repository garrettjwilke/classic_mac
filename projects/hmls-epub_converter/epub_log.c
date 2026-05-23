#include "epub_log.h"
#include <stdio.h>
#include <string.h>

static FILE* log_f = NULL;

void epub_set_vref(short vref) {
    (void)vref;
}

static void log_open(void) {
    if (log_f) {
        fclose(log_f);
        log_f = NULL;
    }
    log_f = fopen("epub.log", "w");
}

static void log_write(const char* msg) {
    if (!msg) return;
    if (!log_f) log_open();
    if (!log_f) return;
    fputs(msg, log_f);
    fputc('\r', log_f);
    fputc('\n', log_f);
    fflush(log_f);
}

void epub_log_init(void) {
    log_open();
    if (log_f) {
        fputs("epub converter log\r\n", log_f);
        fflush(log_f);
    }
}

void epub_log(const char* msg) {
    log_write(msg);
}

void epub_log_status(const char* msg) {
    FILE* st;

    if (!msg) return;
    st = fopen("status.txt", "w");
    if (!st) return;
    fputs(msg, st);
    fputc('\r', st);
    fputc('\n', st);
    fclose(st);
}

void epub_log_mark(const char* name) {
    FILE* f;

    if (!name) return;
    f = fopen(name, "w");
    if (!f) return;
    fputs("ok\r\n", f);
    fclose(f);
}

void epub_log_close(void) {
    if (log_f) {
        fclose(log_f);
        log_f = NULL;
    }
}
