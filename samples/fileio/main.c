// fileio: file input/output on the hard drive
//
// Demonstrates two ways of touching files from OpenXeChain:
//
//  1. Newlib stdio (fopen/fwrite/fread/...) - this is what most code should
//     use. The xbox360 syscall layer routes it through XAM's file API.
//  2. The raw XAM CreateFileA/WriteFile/ReadFile/CloseHandle API.
//
// Both need a hard drive; the "Hdd:" drive letter is a symbolic link to
// "\Device\Harddisk0\Partition1\". On a console without an HDD, the calls
// fail with ERROR_FILE_NOT_FOUND / ERROR_PATH_NOT_FOUND, which the sample
// reports via printf (kernel debug channel).

#include <stdio.h>
#include <string.h>

#include <xecore/xam.h>

#define STDIO_PATH "Hdd:\\OpenXeChain_hello.txt"
#define RAW_PATH   "Hdd:\\OpenXeChain_raw.txt"

static void demo_stdio(void)
{
    const char *message = "Hello from stdio!\n";

    FILE *f = fopen(STDIO_PATH, "w+");
    if (!f)
    {
        printf("stdio: could not open %s\n", STDIO_PATH);
        return;
    }

    fwrite(message, 1, strlen(message), f);
    fseek(f, 0, SEEK_SET);

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';

    fclose(f);
    printf("stdio: wrote and read back %u bytes: %s", (unsigned)n, buf);
}

static void demo_raw_xam(void)
{
    const char *message = "Hello from XAM CreateFileA!\n";
    uint32_t written = 0, read = 0;

    HANDLE h = CreateFileA(RAW_PATH, 0x80000000 | 0x40000000, // GENERIC_READ|GENERIC_WRITE
                           1 | 2,                              // FILE_SHARE_READ|FILE_SHARE_WRITE
                           NULL,
                           2,                                  // CREATE_ALWAYS
                           0x80,                               // FILE_ATTRIBUTE_NORMAL
                           NULL);
    if (h == INVALID_HANDLE_VALUE || h == NULL)
    {
        printf("raw: CreateFileA failed\n");
        return;
    }

    if (!WriteFile(h, (void *)message, (uint32_t)strlen(message), &written, NULL))
        printf("raw: WriteFile failed\n");

    SetFilePointer(h, 0, NULL, 0);

    char buf[256];
    if (!ReadFile(h, buf, sizeof(buf) - 1, &read, NULL))
        printf("raw: ReadFile failed\n");
    buf[read] = '\0';

    CloseHandle(h);
    printf("raw: wrote %u bytes, read back %u bytes: %s", written, read, buf);
}

void main(void)
{
    demo_stdio();
    demo_raw_xam();

    printf("fileio done; files are at:\n  %s\n  %s\n", STDIO_PATH, RAW_PATH);
}
