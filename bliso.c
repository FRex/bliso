#include <Windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

typedef __int64 s64;

static const wchar_t * filepath_to_filename(const wchar_t * path)
{
    size_t i, len, lastslash;

    len = wcslen(path);
    lastslash = 0;
    for(i = 0; i < len; ++i)
        if(path[i] == L'/' || path[i] == L'\\')
            lastslash = i + 1;

    return path + lastslash;
}

static double mytime(void)
{
    LARGE_INTEGER freq, ret;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ret);
    return ((double)ret.QuadPart) / ((double)freq.QuadPart);
}

static int isGoodDiskArg(const wchar_t * argv)
{
    if(wcslen(argv) != 1)
        return 0;

    return 'A' <= argv[0] && argv[0] <= 'Z';
}

static HANDLE openDiskHandle(char disk)
{
    char buff[10];
    buff[0] = '\\';
    buff[1] = '\\';
    buff[2] = '.';
    buff[3] = '\\';
    buff[4] = disk;
    buff[5] = ':';
    buff[6] = '\0';
    return CreateFileA(buff, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0x0, NULL);
}

static HANDLE openWriteFileNoClobber(const wchar_t * fname)
{
    return CreateFileW(fname, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, 0x0, NULL);
}

static void myCloseHandle(HANDLE h, const char * name)
{
    int i;

    for(i = 0; i < 5; ++i)
    {
        if(CloseHandle(h))
            return;

        Sleep(1500);
    }

    fwprintf(stderr, L"FAILED to CloseHandle '%s' 5 times, with Sleep(1500) in  between!\n", name);
}

static unsigned calcbuffsize(unsigned bytespersector)
{
    unsigned ret = bytespersector;

    /* to not have infinite loop below on bad inputs */
    if(bytespersector == 0u)
        return 0u;

    /* < not <= so that as soon as 16 MiB exactly is hit no more is added too */
    while(ret < (16 * 1024 * 1024))
        ret += bytespersector;

    return ret;
}

static int docopy(HANDLE diskhandle, HANDLE isohandle, unsigned bytespersector)
{
    unsigned buffsize;
    DWORD readcount, writecount;
    s64 totalread;
    void * buff;
    double starttime, elapsedtime;

    wprintf(L"bytes per sector: %u\n", bytespersector);
    buffsize = calcbuffsize(bytespersector);
    buff = malloc(buffsize);
    if(!buff)
    {
        fwprintf(stderr, L"malloc(%u) failed\n", buffsize);
        return 1;
    }

    totalread = 0;
    starttime = mytime();
    while(1)
    {
        if(!ReadFile(diskhandle, buff, buffsize, &readcount, NULL))
        {
            fwprintf(stderr, L"ReadFile failed, GetLastError() = %u\n", GetLastError());
            return 1;
        }

        totalread += readcount;
        wprintf(L"read: %u\n", readcount);
        if(readcount == 0u)
            break;

        if(!WriteFile(isohandle, buff, readcount, &writecount, NULL))
        {
            fwprintf(stderr, L"WriteFile failed, GetLastError() = %u\n", GetLastError());
            return 1;
        }

        if(writecount != readcount)
        {
            fwprintf(stderr, L"writecount(%u) != readcount(%u)\n", writecount, readcount);
            return 1;
        }
    } /* while 1 */

    elapsedtime = mytime() - starttime;

    wprintf(L"ALL OK: total read/write: %lld bytes, %.3f MiB, %.3fs, %.3f MiB/s\n",
        totalread, totalread / (1024.0 * 1024.0),
        elapsedtime, totalread / (1024.0 * 1024.0 * elapsedtime)
    );
    free(buff);
    return 0;
}

static int doit(char diskletter, const wchar_t * outfilepath)
{
    HANDLE diskhandle;
    HANDLE isohandle;
    DISK_GEOMETRY geo;
    DWORD unused;
    int ret;

    diskhandle = openDiskHandle(diskletter);
    if(diskhandle == INVALID_HANDLE_VALUE)
    {
        fwprintf(stderr, L"openDiskHandle('%c') returned INVALID_HANDLE_VALUE, GetLastError() = %u\n", diskletter, GetLastError());
        return 1;
    }

    if(!DeviceIoControl(diskhandle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo, sizeof(geo), &unused, NULL))
    {
        fwprintf(stderr, L"DeviceIoControl return false, GetLastError() = %u\n", GetLastError());
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    if(geo.MediaType != RemovableMedia)
    {
        fwprintf(stderr, L"geo.MediaType is not RemovableMedia\n");
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    if(geo.BytesPerSector == 0u)
    {
        fwprintf(stderr, L"geo.BytesPerSector is 0\n");
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    isohandle = openWriteFileNoClobber(outfilepath);
    if(isohandle == INVALID_HANDLE_VALUE)
    {
        const DWORD lasterror = GetLastError();
        if(lasterror == ERROR_FILE_EXISTS)
        {
            fwprintf(stderr, L"%ls - file already exists (GetLastError() == %u == ERROR_FILE_EXISTS), refusing to clobber\n",
                outfilepath, lasterror);
        }
        else
        {
            fwprintf(stderr, L"openWriteFileNoClobber(\"%ls\") returned INVALID_HANDLE_VALUE, GetLastError() = %u\n",
                outfilepath, lasterror);
        }

        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    ret = docopy(diskhandle, isohandle, geo.BytesPerSector);
    myCloseHandle(diskhandle, "diskhandle");
    myCloseHandle(isohandle, "isohandle");
    return ret;
}

int wmain(int argc, wchar_t ** argv)
{
    if(argc != 3)
    {
        const wchar_t * fname = filepath_to_filename(argv[0]);
        fwprintf(stderr, L"%ls - rip a CD/DVD to an iso file\n", fname);
        fwprintf(stderr, L"Usage (rip): %ls diskletter isofile\n", fname);
        return 1;
    }

    if(!isGoodDiskArg(argv[1]))
    {
        fwprintf(stderr, L"'%ls' is not a good disk letter (uppercase A-Z only, no : or / or \\)", argv[1]);
        return 1;
    }

    /* this access and char cast is safe after isGoodDiskArg returned true */
    return doit((char)argv[1][0], argv[2]);
}
