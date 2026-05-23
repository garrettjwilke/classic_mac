#ifndef EPUB_LOG_H
#define EPUB_LOG_H

void epub_set_vref(short vref);
void epub_log_init(void);
void epub_log(const char* msg);
void epub_log_mark(const char* name);
void epub_log_status(const char* msg);
void epub_log_close(void);

#endif
