#ifndef RETROBURNER_PLATFORM_H
#define RETROBURNER_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <io.h>

static inline int
rb_sleep_us(uint64_t microseconds)
{
    uint64_t milliseconds = (microseconds + UINT64_C(999)) / UINT64_C(1000);

    while (milliseconds > UINT64_C(0xFFFFFFFE)) {
        Sleep((DWORD)0xFFFFFFFEu);
        milliseconds -= UINT64_C(0xFFFFFFFE);
    }

    Sleep((DWORD)milliseconds);
    return 0;
}

static inline unsigned int
rb_sleep_seconds(unsigned int seconds)
{
    return rb_sleep_us((uint64_t)seconds * UINT64_C(1000000)) == 0
        ? 0u
        : seconds;
}

static inline size_t
rb_page_size(void)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return (size_t)info.dwPageSize;
}

/*
 * Return the size of a regular file referenced by a CRT file descriptor.
 *
 * Do not use 32-bit CRT stat/off_t as the Windows i686 backend must handle
 * DVD/BD images larger than 4 GiB. GetFileSizeEx uses a 64-bit LARGE_INTEGER.
 */
static inline int
rb_regular_file_size_fd(int fd, int64_t *size_out)
{
    intptr_t native;
    HANDLE handle;
    LARGE_INTEGER size;

    if (size_out == NULL)
        return 0;

    native = _get_osfhandle(fd);
    if (native == (intptr_t)-1)
        return 0;

    handle = (HANDLE)native;

    if (GetFileType(handle) != FILE_TYPE_DISK)
        return 0;

    if (!GetFileSizeEx(handle, &size))
        return 0;

    if (size.QuadPart < 0)
        return 0;

    *size_out = (int64_t)size.QuadPart;
    return 1;
}

#else

#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static inline int
rb_sleep_us(uint64_t microseconds)
{
    struct timespec request;
    struct timespec remaining;

    request.tv_sec = (time_t)(microseconds / UINT64_C(1000000));
    request.tv_nsec = (long)((microseconds % UINT64_C(1000000)) * UINT64_C(1000));

    while (nanosleep(&request, &remaining) != 0) {
        if (errno != EINTR)
            return -1;

        request = remaining;
    }

    return 0;
}

static inline unsigned int
rb_sleep_seconds(unsigned int seconds)
{
    return rb_sleep_us((uint64_t)seconds * UINT64_C(1000000)) == 0
        ? 0u
        : seconds;
}

static inline size_t
rb_page_size(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    return page_size > 0 ? (size_t)page_size : (size_t)4096;
}

/*
 * POSIX implementation. RetroBurner Linux builds will enforce a 64-bit
 * off_t through CMake; modern macOS already provides 64-bit off_t.
 */
static inline int
rb_regular_file_size_fd(int fd, int64_t *size_out)
{
    struct stat st;

    if (size_out == NULL)
        return 0;

    if (fstat(fd, &st) != 0)
        return 0;

    if (!S_ISREG(st.st_mode) || st.st_size < 0)
        return 0;

    *size_out = (int64_t)st.st_size;
    return 1;
}

#endif /* _WIN32 */

#endif /* RETROBURNER_PLATFORM_H */