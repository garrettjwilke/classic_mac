#include "epub_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char* epub_decode_url(const char* url) {
    char* ret;
    int len = 0;

    if (!url) {
        return NULL;
    }

    ret = (char*)malloc(strlen(url) + 2);
    if (!ret) {
        return NULL;
    }

    for (; *url; len++) {
        if (*url == '%' && url[1] && url[2] && isxdigit((unsigned char)url[1])
            && isxdigit((unsigned char)url[2])) {
            char url1 = url[1];
            char url2 = url[2];

            url1 -= url1 <= '9' ? '0' : (url1 <= 'F' ? 'A' : 'a') - 10;
            url2 -= url2 <= '9' ? '0' : (url2 <= 'F' ? 'A' : 'a') - 10;
            ret[len] = (char)(16 * url1 + url2);
            url += 3;
            continue;
        }
        if (*url == '+') {
            url += 1;
            ret[len] = ' ';
            continue;
        }
        ret[len] = *url++;
    }
    ret[len] = '\0';
    return ret;
}
