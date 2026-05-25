#include "xhtml_lite.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int my_strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n > 0) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        if (tolower(c1) != tolower(c2)) {
            return tolower(c1) - tolower(c2);
        }
        if (c1 == '\0') {
            return 0;
        }
        n--;
    }
    return 0;
}

static int tag_is_block(const char* p) {
    if (my_strncasecmp(p, "<p", 2) == 0 || my_strncasecmp(p, "<div", 4) == 0
        || my_strncasecmp(p, "<br", 3) == 0 || my_strncasecmp(p, "<h", 2) == 0
        || my_strncasecmp(p, "<li", 3) == 0 || my_strncasecmp(p, "<tr", 3) == 0
        || my_strncasecmp(p, "<blockquote", 12) == 0 || my_strncasecmp(p, "<section", 8) == 0) {
        return 1;
    }
    return 0;
}

static int tag_is_skip(const char* p) {
    if (my_strncasecmp(p, "<script", 7) == 0 || my_strncasecmp(p, "<style", 6) == 0
        || my_strncasecmp(p, "<!--", 4) == 0) {
        return 1;
    }
    return 0;
}

char* find_rootfile(const char* xml) {
    const char* p = strstr(xml, "full-path=");
    const char* end;
    size_t len;
    char* res;

    if (!p) {
        return NULL;
    }
    p += 10;
    {
        char quote = *p;
        if (quote != '"' && quote != '\'') {
            return NULL;
        }
        p++;
        end = strchr(p, quote);
        if (!end) {
            return NULL;
        }
        len = (size_t)(end - p);
        res = (char*)malloc(len + 1);
        if (!res) {
            return NULL;
        }
        memcpy(res, p, len);
        res[len] = '\0';
        return res;
    }
}

void find_spine_items(const char* xml, SpineItemCallback cb, void* user_data) {
    const char* manifest = strstr(xml, "<manifest>");
    const char* manifest_end;
    const char* spine;
    const char* spine_end;
    const char* p;

    if (!manifest) {
        return;
    }
    manifest_end = strstr(manifest, "</manifest>");
    if (!manifest_end) {
        return;
    }

    spine = strstr(xml, "<spine");
    if (!spine) {
        return;
    }
    spine_end = strstr(spine, "</spine>");
    if (!spine_end) {
        return;
    }

    p = spine;
    while (p < spine_end) {
        const char* idref_p;

        p = strstr(p, "<itemref");
        if (!p || p >= spine_end) {
            break;
        }

        idref_p = strstr(p, "idref=");
        if (idref_p && idref_p < strchr(p, '>')) {
            char quote;
            const char* idref_end;
            size_t id_len;

            idref_p += 6;
            quote = *idref_p;
            idref_p++;
            idref_end = strchr(idref_p, quote);
            if (idref_end) {
                char id[128];
                char search[256];
                const char* item_p;

                id_len = (size_t)(idref_end - idref_p);
                if (id_len < 127) {
                    memcpy(id, idref_p, id_len);
                    id[id_len] = '\0';

                    sprintf(search, "id=\"%s\"", id);
                    item_p = strstr(manifest, search);
                    if (!item_p || item_p > manifest_end) {
                        sprintf(search, "id='%s'", id);
                        item_p = strstr(manifest, search);
                    }

                    if (item_p && item_p < manifest_end) {
                        const char* item_start = item_p;
                        const char* href_p;

                        while (item_start > manifest && *item_start != '<') {
                            item_start--;
                        }

                        href_p = strstr(item_start, "href=");
                        if (href_p && href_p < strchr(item_start, '>')) {
                            char q2;
                            const char* href_end;
                            size_t h_len;
                            char* href;

                            href_p += 5;
                            q2 = *href_p;
                            href_p++;
                            href_end = strchr(href_p, q2);
                            if (href_end) {
                                h_len = (size_t)(href_end - href_p);
                                href = (char*)malloc(h_len + 1);
                                if (href) {
                                    memcpy(href, href_p, h_len);
                                    href[h_len] = '\0';
                                    cb(href, user_data);
                                    free(href);
                                }
                            }
                        }
                    }
                }
            }
        }
        p = strchr(p, '>');
        if (!p) {
            break;
        }
        p++;
    }
}

void xhtml_parser_init(XHTML_Parser* parser, XHTMLTextCallback cb, void* user_data) {
    parser->in_tag = 0;
    parser->in_body = 0;
    parser->has_checked_body = 0;
    parser->skip_until_gt = 0;
    parser->cb = cb;
    parser->user_data = user_data;
}

static void emit_entity(XHTML_Parser* parser, const char* p) {
    if (my_strncasecmp(p, "&nbsp;", 6) == 0) {
        parser->cb(" ", parser->user_data);
    } else if (my_strncasecmp(p, "&lt;", 4) == 0) {
        parser->cb("<", parser->user_data);
    } else if (my_strncasecmp(p, "&gt;", 4) == 0) {
        parser->cb(">", parser->user_data);
    } else if (my_strncasecmp(p, "&amp;", 5) == 0) {
        parser->cb("&", parser->user_data);
    } else if (my_strncasecmp(p, "&quot;", 6) == 0) {
        parser->cb("\"", parser->user_data);
    } else if (my_strncasecmp(p, "&apos;", 6) == 0) {
        parser->cb("'", parser->user_data);
    } else if (my_strncasecmp(p, "&#", 2) == 0) {
        long code = 0;
        const char* q = p + 2;
        char buf[8];

        if (*q == 'x' || *q == 'X') {
            q++;
            while ((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F')) {
                code = code * 16 + (*q <= '9' ? *q - '0' : (*q & 0x0F) + 9);
                q++;
            }
        } else {
            while (*q >= '0' && *q <= '9') {
                code = code * 10 + (*q - '0');
                q++;
            }
        }
        if (code > 0 && code < 128) {
            buf[0] = (char)code;
            buf[1] = '\0';
            parser->cb(buf, parser->user_data);
        }
    } else {
        char buf[2] = {*p, '\0'};
        parser->cb(buf, parser->user_data);
    }
}

void xhtml_parser_process(XHTML_Parser* parser, const char* chunk, size_t size) {
    const char* p = chunk;
    const char* end = chunk + size;

    if (!parser->has_checked_body) {
        if (!strstr(chunk, "<body") && !strstr(chunk, "<BODY")) {
            if (my_strncasecmp(chunk, "<?xml", 5) != 0 && !strstr(chunk, "<html")) {
                parser->in_body = 1;
            }
        }
        parser->has_checked_body = 1;
    }

    while (p < end) {
        if (parser->skip_until_gt) {
            if (*p == '>') {
                parser->skip_until_gt = 0;
                parser->in_tag = 0;
            }
            p++;
            continue;
        }

        if (*p == '<') {
            parser->in_tag = 1;
            if (my_strncasecmp(p, "<body", 5) == 0) {
                parser->in_body = 1;
            }
            if (my_strncasecmp(p, "</body", 6) == 0) {
                parser->in_body = 0;
            }
            if (tag_is_skip(p)) {
                parser->skip_until_gt = 1;
            } else if (parser->in_body && tag_is_block(p)) {
                parser->cb("\r", parser->user_data);
            }
        } else if (*p == '>') {
            parser->in_tag = 0;
        } else if (!parser->in_tag && parser->in_body) {
            if (*p == '&') {
                emit_entity(parser, p);
            } else if (*p != '\r' && *p != '\n') {
                char buf[2] = {*p, '\0'};
                parser->cb(buf, parser->user_data);
            } else if (*p == '\n' || *p == '\r') {
                parser->cb(" ", parser->user_data);
            }
        }
        p++;
    }
}
