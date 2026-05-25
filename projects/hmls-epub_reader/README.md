# hmls-epub_reader

Native EPUB reader for Classic Macintosh System 6, built with [Retro68](https://github.com/autc04/Retro68).

## Features

- Open `.epub` files directly on the Mac (no shell `unzip`)
- In-process ZIP + deflate (`zip_lite` + `puff`)
- OPF spine → chapters; paginated reading with prev/next and page numbers
- Chapters menu to jump to the first page of each spine item
- Sidecar files beside the EPUB: `.book` (plain text), `.pgdata` (page index), `.read` (progress)

## Build

From the `classic_mac` repository root (with `Retro68-build/` present):

```bash
./build.sh projects/hmls-epub_reader
```

Output: `target/hmls-epub_reader/hmls-epub_reader.APPL` and a `.dsk` in `target/00_floppy_images/`.

## Usage

1. Copy the app (and optionally an EPUB) onto a System 6 disk image.
2. Launch **EPUB Reader**.
3. **File → Open EPUB...** and select an ebook.
4. Wait for import and index build (progress dialog).
5. Read with Page Left/Right, arrows, or **Go To Page**.
6. **Chapters** menu lists spine items; choose one to jump.

## Limits (Mini vMac / 512K–1MB Mac)

| Limit | Detail |
|-------|--------|
| Deflate chapter size | ZIP entries with uncompressed size **> 300 KB** are skipped (see `zip_lite.c`) |
| Spine items | Max **64** chapters (`kMaxChapters`) |
| UTF-8 | Text is stored as UTF-8 bytes; Geneva 12 shows approximate Latin glyphs |
| EPUB 3 nav / NCX | Reading order follows **OPF spine only** |
| Re-import | If `.book` exists and is newer than the EPUB, import is skipped; delete `.book`/`.pgdata` to force rebuild |

## Format

`.pgdata` version 4 (`book_format.h`): 24-byte header, chapter table, title blob, then page byte offsets.

## Test EPUB

A minimal two-chapter sample is in `test-books/test_reader.epub` (about 1.7 KB). Copy it onto the same volume as the app, then use **File → Open EPUB...**.

## Mini vMac

After `./build.sh projects/hmls-epub_reader`, use:

- `target/hmls-epub_reader/hmls-epub_reader.bin` (MacBinary), or
- `target/hmls-epub_reader/hmls-epub_reader.dsk` (boot disk; may need a larger image if `notEnoughSpace` when copying to `00_floppy_images/`)

## Host tool (optional)

The `hmls-ebook_reader/tools/mkbook` host utility builds v3 indexes only. This app builds **v4** indexes (with chapters) on the Mac during reading.
