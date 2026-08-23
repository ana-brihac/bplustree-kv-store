#ifndef BPLUSTREE_COMPAT_H
#define BPLUSTREE_COMPAT_H

/* Windows (mingw-w64) has no native fsync(); _commit() is its equivalent.
 * Include this header AFTER <unistd.h>/<io.h> in each translation unit. */
#if defined(_WIN32) || defined(__MINGW32__)
#include <io.h>
static inline int fsync(int fd) { return _commit(fd); }
#endif

/* POSIX rename() atomically replaces an existing destination file.
 * Windows rename() does NOT: it fails (and leaves the destination
 * untouched) if the destination already exists, so callers relying on
 * atomic-replace-on-rename must use this instead. */
#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>
static inline int atomic_replace(const char *from, const char *to) {
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
}
#else
#include <stdio.h>
static inline int atomic_replace(const char *from, const char *to) {
    return rename(from, to);
}
#endif

#endif
