# Platform portability (Windows/POSIX)

This project builds on both POSIX (Linux/macOS) and Windows (via
mingw-w64 `gcc`). Until the 2026-08-23 fix (`36c70de`) it did not even
**link** on Windows; treat this area as young and under-exercised
compared to the POSIX path.

## `src/compat.h` — the shim layer

Two gaps between POSIX and Windows (mingw), both worked around here:

```c
/* Windows (mingw-w64) has no native fsync(); _commit() is its equivalent. */
#if defined(_WIN32) || defined(__MINGW32__)
#include <io.h>
static inline int fsync(int fd) { return _commit(fd); }
#endif

/* POSIX rename() atomically replaces an existing destination file.
 * Windows rename() does NOT — it fails if the destination exists. */
#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>
static inline int atomic_replace(const char *from, const char *to) {
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
}
#else
static inline int atomic_replace(const char *from, const char *to) {
    return rename(from, to);
}
#endif
```

**Must be included *after* `<unistd.h>`/`<io.h>` in each translation
unit** (the comment in the header says so explicitly) — the `fsync`
shim needs `_commit` from `<io.h>` in scope, and defining it too early
can clash with declarations pulled in later.

`atomic_replace` is currently only used by test infrastructure
(`tests/crash_cycle.c`, for atomically swapping in an updated tracker
file) — not by any `src/` code yet. If you add durable "swap a temp file
into place" logic to the engine itself (e.g. atomic metadata file
replacement), use `atomic_replace`, not raw `rename()`.

## `O_BINARY`

```c
#ifndef O_BINARY
#define O_BINARY 0 /* no-op on POSIX; required on Windows so raw page/WAL
                       bytes aren't CRLF-translated */
#endif
```
Defined identically in both `src/page_manager.c` and `src/wal.c`, and
OR'd into every `open()` call for the DB file and the WAL file. Windows'
C runtime opens files in **text mode** by default, which silently
rewrites `\n` <-> `\r\n` on read/write. Since both the DB file and the
WAL file are raw binary page dumps (see
[on-disk-format.md](on-disk-format.md) and
[wal-recovery.md](wal-recovery.md)), any byte in a page that happened to
be `0x0A` could get corrupted into a two-byte sequence on Windows without
`O_BINARY`.

**This was not a hypothetical risk** — it was the actual root cause of
`test_checkpoint`'s byte-count mismatch and `test_reopen`'s invariant
failure on Windows before the fix. If you add any new file that stores
raw binary engine data, `O_BINARY` it.

## Practical implications

- Building on Windows requires mingw-w64 `gcc` (e.g. via MSYS2), not
  MSVC — `compat.h`'s Windows branch assumes mingw's `<io.h>`/`_commit`
  and the standard Win32 `<windows.h>` API, and the `Makefile` assumes a
  Unix-shell-compatible `make`.
- `-fsanitize=address,undefined` (see
  [build-environment.md](build-environment.md)) works under mingw-w64 but
  is generally less mature there than on Linux/macOS — if ASan reports
  something on Windows that looks environment-specific, reproduce on
  Linux/macOS before assuming it's a real engine bug.
- Database/WAL files are **not** portable between a POSIX-built binary
  and a Windows-built binary regardless of `O_BINARY` — the on-disk
  format is a raw struct memcpy (see
  [on-disk-format.md](on-disk-format.md)), so ABI/compiler differences
  matter independently of the text/binary-mode issue this file covers.
