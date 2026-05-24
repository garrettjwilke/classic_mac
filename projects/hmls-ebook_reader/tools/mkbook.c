/*
 * mkbook — build .pgdata page-index files for hmls-ebook_reader on modern hosts.
 *
 * Usage:
 *   mkbook [options] input.txt [output.pgdata]
 *
 * Build (standalone, from this directory):
 *   cd projects/hmls-ebook_reader/tools
 *   cmake -B build
 *   cmake --build build
 */

#include "book_format.h"
#include "paginate.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [options] input.txt [output.pgdata]\n"
        "\n"
        "Build a .pgdata page index for hmls-ebook_reader (Classic Mac).\n"
        "\n"
        "Options:\n"
        "  -o PATH       Output .pgdata file (default: input with .pgdata extension)\n"
        "  -l LINES      Lines per page (default: full-screen reader layout)\n"
        "  -H HEIGHT     Line height in pixels (default: %d)\n"
        "  -W WIDTH      Max line width in pixels (default: full-screen reader layout)\n"
        "  -q            Quiet (no progress on stderr)\n"
        "  -h            Show this help\n",
        prog,
        kDefaultGenevaLineHeight);
}

static char* read_file(const char* path, long* outLen) {
    FILE* f;
    unsigned char* buf;
    long n;

    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buf = (unsigned char*)malloc((size_t)n + 1U);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    buf[n] = '\0';
    *outLen = n;
    return (char*)buf;
}

static int write_be32(FILE* f, int32_t v) {
    int32_t be = be32(v);
    return fwrite(&be, 1, 4, f) == 4 ? 0 : -1;
}

static int write_header(FILE* f, const BookHeader* hdr) {
    BookHeader be;

    book_header_to_be(hdr, &be);
    return fwrite(&be, 1, kBookHeaderSize, f) == (size_t)kBookHeaderSize ? 0 : -1;
}

static char* default_book_path(const char* txtPath) {
    size_t len = strlen(txtPath);
    char* out;
    const char* dot = strrchr(txtPath, '.');

    if (dot && dot != txtPath) {
        len = (size_t)(dot - txtPath);
    }

    out = (char*)malloc(len + 8U);
    if (!out) {
        return NULL;
    }
    memcpy(out, txtPath, len);
    memcpy(out + len, ".pgdata", 7);
    return out;
}

static int build_book(const char* txtPath, const char* bookPath, BookLayout* layout, int quiet) {
    char* data;
    long sourceLen;
    FILE* out;
    PaginateCtx ctx;
    long pos = 0;
    long pageCount = 0;
    BookHeader hdr;

    data = read_file(txtPath, &sourceLen);
    if (!data) {
        fprintf(stderr, "mkbook: cannot read \"%s\": %s\n", txtPath, strerror(errno));
        return 1;
    }

    paginate_init(&ctx, (const unsigned char*)data, sourceLen, layout);

    out = fopen(bookPath, "wb");
    if (!out) {
        fprintf(stderr, "mkbook: cannot write \"%s\": %s\n", bookPath, strerror(errno));
        free(data);
        return 1;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = kBookMagic;
    hdr.version = (int16_t)kBookVersion;
    hdr.linesPerPage = layout->linesPerPage;
    hdr.lineHeight = layout->lineHeight;
    hdr.maxPixelWidth = layout->maxPixelWidth;
    hdr.sourceLen = (int32_t)sourceLen;
    hdr.sourceModDate = 0;
    hdr.pageCount = 0;

    if (write_header(out, &hdr) != 0) {
        fprintf(stderr, "mkbook: write failed\n");
        fclose(out);
        free(data);
        return 1;
    }

    while (pos < sourceLen) {
        if (write_be32(out, (int32_t)pos) != 0) {
            fprintf(stderr, "mkbook: write failed\n");
            fclose(out);
            free(data);
            return 1;
        }

        pageCount++;
        if (!quiet && (pageCount == 1 || (pageCount % 100) == 0)) {
            fprintf(stderr, "Indexing page %ld...\r", pageCount);
            fflush(stderr);
        }

        if (pos >= sourceLen) {
            break;
        }

        {
            long next = paginate_advance_page(&ctx, pos);
            if (next <= pos) {
                break;
            }
            pos = next;
        }
    }

    if (!quiet) {
        fprintf(stderr, "Indexing page %ld — done.   \n", pageCount);
    }

    hdr.pageCount = (int32_t)pageCount;
    if (fseek(out, 0, SEEK_SET) != 0 || write_header(out, &hdr) != 0) {
        fprintf(stderr, "mkbook: failed to finalize header\n");
        fclose(out);
        free(data);
        return 1;
    }

    fclose(out);
    free(data);

    if (!quiet) {
        printf("Wrote %s (%ld pages, %ld bytes source)\n", bookPath, pageCount, sourceLen);
        printf("  layout: %d lines/page, line height %d px, max width %d px\n",
            (int)layout->linesPerPage,
            (int)layout->lineHeight,
            (int)layout->maxPixelWidth);
    }

    return 0;
}

int main(int argc, char** argv) {
    const char* input = NULL;
    const char* output = NULL;
    char* outputOwned = NULL;
    BookLayout layout;
    int quiet = 0;
    int i;

    book_layout_defaults(&layout);

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (!input) {
                input = argv[i];
            } else if (!output) {
                output = argv[i];
            } else {
                usage(argv[0]);
                return 1;
            }
            continue;
        }

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-q") == 0) {
            quiet = 1;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            layout.linesPerPage = (int16_t)atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-H") == 0 && i + 1 < argc) {
            layout.lineHeight = (int16_t)atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-W") == 0 && i + 1 < argc) {
            layout.maxPixelWidth = (int16_t)atoi(argv[++i]);
            continue;
        }

        fprintf(stderr, "mkbook: unknown option \"%s\"\n", argv[i]);
        usage(argv[0]);
        return 1;
    }

    if (!input) {
        usage(argv[0]);
        return 1;
    }

    if (!output) {
        outputOwned = default_book_path(input);
        if (!outputOwned) {
            fprintf(stderr, "mkbook: out of memory\n");
            return 1;
        }
        output = outputOwned;
    }

    if (layout.linesPerPage < 1) {
        fprintf(stderr, "mkbook: lines per page must be >= 1\n");
        free(outputOwned);
        return 1;
    }

    i = build_book(input, output, &layout, quiet);
    free(outputOwned);
    return i;
}
