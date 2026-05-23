#include "xhtml_lite.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static int my_strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        if (tolower(c1) != tolower(c2)) return tolower(c1) - tolower(c2);
        if (c1 == '\0') return 0;
        n--;
    }
    return 0;
}

char* find_rootfile(const char* xml) {
    const char* p = strstr(xml, "full-path=");
    if (!p) return NULL;
    p += 10;
    char quote = *p;
    if (quote != '"' && quote != '\'') return NULL;
    p++;
    const char* end = strchr(p, quote);
    if (!end) return NULL;
    size_t len = end - p;
    char* res = malloc(len + 1);
    memcpy(res, p, len);
    res[len] = '\0';
    return res;
}

void find_spine_items(const char* xml, SpineItemCallback cb, void* user_data) {
    const char* manifest = strstr(xml, "<manifest>");
    if (!manifest) { printf("    Error: <manifest> not found.\n"); return; }
    const char* manifest_end = strstr(manifest, "</manifest>");
    if (!manifest_end) { printf("    Error: </manifest> not found.\n"); return; }

    const char* spine = strstr(xml, "<spine");
    if (!spine) { printf("    Error: <spine> not found.\n"); return; }
    const char* spine_end = strstr(spine, "</spine>");
    if (!spine_end) { printf("    Error: </spine> not found.\n"); return; }

    printf("    Scanning spine items...\n");
    const char* p = spine;
    int items_found = 0;
    while (p < spine_end) {
        p = strstr(p, "<itemref");
        if (!p || p >= spine_end) break;
        
        const char* idref_p = strstr(p, "idref=");
        if (idref_p && idref_p < strchr(p, '>')) {
            idref_p += 6;
            char quote = *idref_p;
            idref_p++;
            const char* idref_end = strchr(idref_p, quote);
            if (idref_end) {
                size_t id_len = idref_end - idref_p;
                char id[128];
                if (id_len < 127) {
                    memcpy(id, idref_p, id_len);
                    id[id_len] = '\0';
                    
                    char search[256];
                    sprintf(search, "id=\"%s\"", id);
                    const char* item_p = strstr(manifest, search);
                    if (!item_p || item_p > manifest_end) {
                         sprintf(search, "id='%s'", id);
                         item_p = strstr(manifest, search);
                    }
                    
                    if (item_p && item_p < manifest_end) {
                        const char* item_start = item_p;
                        while (item_start > manifest && *item_start != '<') item_start--;
                        
                        const char* href_p = strstr(item_start, "href=");
                        if (href_p && href_p < strchr(item_start, '>')) {
                            href_p += 5;
                            char q2 = *href_p;
                            href_p++;
                            const char* href_end = strchr(href_p, q2);
                            if (href_end) {
                                size_t h_len = href_end - href_p;
                                char* href = malloc(h_len + 1);
                                memcpy(href, href_p, h_len);
                                href[h_len] = '\0';
                                items_found++;
                                cb(href, user_data);
                                free(href);
                            }
                        }
                    }
                }
            }
        }
        p = strchr(p, '>');
        if (!p) break;
        p++;
    }
    printf("    Finished scanning spine. Found %d items.\n", items_found);
}

void xhtml_parser_init(XHTML_Parser* parser, XHTMLTextCallback cb, void* user_data) {
    parser->in_tag = 0;
    parser->in_body = 0;
    parser->has_checked_body = 0;
    parser->cb = cb;
    parser->user_data = user_data;
}

void xhtml_parser_process(XHTML_Parser* parser, const char* chunk, size_t size) {
    const char* p = chunk;
    const char* end = chunk + size;

    if (!parser->has_checked_body) {
        if (!strstr(chunk, "<body") && !strstr(chunk, "<BODY")) {
            // Very simple heuristic: if we don't see body in first chunk, maybe it's missing or further down.
            // But if it's not HTML at all, we want to see it.
            if (my_strncasecmp(chunk, "<?xml", 5) != 0 && !strstr(chunk, "<html")) {
                parser->in_body = 1;
            }
        }
        parser->has_checked_body = 1;
    }

    while (p < end) {
        if (*p == '<') {
            parser->in_tag = 1;
            if (my_strncasecmp(p, "<body", 5) == 0) parser->in_body = 1;
            if (my_strncasecmp(p, "</body", 6) == 0) parser->in_body = 0;
            
            if (parser->in_body) {
                if (my_strncasecmp(p, "<p", 2) == 0 || my_strncasecmp(p, "<div", 4) == 0 || 
                    my_strncasecmp(p, "<br", 3) == 0 || my_strncasecmp(p, "<h", 2) == 0 ||
                    my_strncasecmp(p, "<li>", 4) == 0 || my_strncasecmp(p, "<tr>", 4) == 0) {
                    parser->cb("\r", parser->user_data);
                }
            }
        } else if (*p == '>') {
            parser->in_tag = 0;
        } else if (!parser->in_tag && parser->in_body) {
            if (*p == '&') {
                if (my_strncasecmp(p, "&nbsp;", 6) == 0) { parser->cb(" ", parser->user_data); p += 5; }
                else if (my_strncasecmp(p, "&lt;", 4) == 0) { parser->cb("<", parser->user_data); p += 3; }
                else if (my_strncasecmp(p, "&gt;", 4) == 0) { parser->cb(">", parser->user_data); p += 3; }
                else if (my_strncasecmp(p, "&amp;", 5) == 0) { parser->cb("&", parser->user_data); p += 4; }
                else if (my_strncasecmp(p, "&quot;", 6) == 0) { parser->cb("\"", parser->user_data); p += 5; }
                else if (my_strncasecmp(p, "&apos;", 6) == 0) { parser->cb("'", parser->user_data); p += 5; }
                else { char buf[2] = {*p, 0}; parser->cb(buf, parser->user_data); }
            } else {
                if (*p != '\r' && *p != '\n') {
                   char buf[2] = {*p, 0};
                   parser->cb(buf, parser->user_data);
                } else if (*p == '\n' || *p == '\r') {
                    parser->cb(" ", parser->user_data);
                }
            }
        }
        p++;
    }
}
