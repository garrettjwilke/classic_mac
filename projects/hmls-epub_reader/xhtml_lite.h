#ifndef XHTML_LITE_H
#define XHTML_LITE_H

#include <stddef.h>

typedef void (*XHTMLTextCallback)(const char* text, void* user_data);

typedef struct {
    int in_tag;
    int in_body;
    int has_checked_body;
    int skip_until_gt;
    XHTMLTextCallback cb;
    void* user_data;
} XHTML_Parser;

void xhtml_parser_init(XHTML_Parser* parser, XHTMLTextCallback cb, void* user_data);
void xhtml_parser_process(XHTML_Parser* parser, const char* chunk, size_t size);

char* find_rootfile(const char* xml);

typedef void (*SpineItemCallback)(const char* href, void* user_data);
void find_spine_items(const char* opf_xml, SpineItemCallback cb, void* user_data);

#endif
