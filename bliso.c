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

        fwprintf(stderr, L"Failed to CloseHandle '%s' on attempt %d, GetLastError() = %d\n", name, i, GetLastError());
        Sleep(1500);
    }

    fwprintf(stderr, L"FAILED to CloseHandle '%s' 5 times, with Sleep(1500) in  between, giving up!\n", name);
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

static int printFuncErr(const char * funcname)
{
    fwprintf(stderr, L"%s failed, GetLastError() = %u\n", funcname, GetLastError());
    return 1;
}

static double toMiB(s64 size)
{
    return size / (1024.0 * 1024.0);
}

static void printProgress(double starttime, s64 totalread, s64 desiredsize)
{
    const double elapsedtime = mytime() - starttime;
    const double mib = toMiB(totalread);
    const double percent = 100.0 * (totalread / (double)desiredsize);
    wprintf(L"PROGRESS: %lld/%lld, %0.1f%%, %.3f MiB, %.1fs, %.3f MiB/s\n",
        totalread, desiredsize, percent, mib, elapsedtime, mib / elapsedtime);
}

static int docopy(HANDLE diskhandle, HANDLE isohandle, unsigned bytespersector, s64 desiredsize)
{
    unsigned buffsize;
    DWORD readcount, writecount;
    s64 totalread;
    void * buff;
    double starttime;

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
            return printFuncErr("ReadFile");

        totalread += readcount;
        if(readcount == 0u)
            break;

        if(!WriteFile(isohandle, buff, readcount, &writecount, NULL))
            return printFuncErr("WriteFile");

        if(writecount != readcount)
        {
            fwprintf(stderr, L"writecount(%u) != readcount(%u)\n", writecount, readcount);
            return 1;
        }

        printProgress(starttime, totalread, desiredsize);
    } /* while 1 */

    memset(buff, 0x0, buffsize);
    while(totalread < desiredsize)
    {
        const s64 need = desiredsize - totalread;
        readcount = (need < ((s64)buffsize)) ? ((unsigned)need) : buffsize;

        totalread += readcount;

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
    }

    printProgress(starttime, totalread, desiredsize);
    free(buff);
    return 0;
}

static int isDiskAvailable(char upperLetter)
{
    if(!('A' <= upperLetter && upperLetter <= 'Z'))
        return 0;

    const DWORD d = GetLogicalDrives();
    return (d >> (upperLetter - 'A')) & 1;
}

static int doit(char diskletter, const wchar_t * outfilepath, FILE * discerrto)
{
    HANDLE diskhandle;
    HANDLE isohandle;
    DISK_GEOMETRY_EX geo;
    DWORD unused;
    int ret;
    wchar_t name[512];

    if(!isDiskAvailable(diskletter))
    {
        fwprintf(discerrto, L"Disc %c: bit not set in GetLogicalDrives()\n", diskletter);
        return 1;
    }

    diskhandle = openDiskHandle(diskletter);
    if(diskhandle == INVALID_HANDLE_VALUE)
    {
        fwprintf(discerrto, L"Disc %c: openDiskHandle returned INVALID_HANDLE_VALUE, GetLastError() = %u\n",
            diskletter, GetLastError());
        return 1;
    }

    if(!DeviceIoControl(diskhandle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geo, sizeof(geo), &unused, NULL))
    {
        const DWORD lasterror = GetLastError();
        if(lasterror == ERROR_NOT_READY)
        {
            fwprintf(discerrto,
                L"Disc %c: DeviceIoControl returned false, GetLastError() == %u == ERROR_NOT_READY, no disc in drive?\n",
                diskletter, lasterror
            );
        }
        else
        {
            fwprintf(discerrto, L"Disc %c: DeviceIoControl returned false, GetLastError() = %u\n", diskletter, lasterror);
        }
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    if(geo.Geometry.MediaType != RemovableMedia)
    {
        fwprintf(discerrto, L"Disc %c: geo.Geometry.MediaType is %d != RemovableMedia (%d)\n",
            diskletter, (int)geo.Geometry.MediaType, (int)RemovableMedia);
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    if(geo.Geometry.BytesPerSector == 0u)
    {
        fwprintf(discerrto, L"Disc %c: geo.Geometry.BytesPerSector is 0\n", diskletter);
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    memset(name, 0x0, sizeof(wchar_t) * 512);
    if(!GetVolumeInformationByHandleW(diskhandle, name, 510, NULL, NULL, NULL, NULL, 0))
    {
        fwprintf(discerrto, L"Disc %c: GetVolumeInformationByHandleW failed, GetLastError() = %u\n", diskletter, GetLastError());
        myCloseHandle(diskhandle, "diskhandle");
        return 1;
    }

    wprintf(L"Disc %c: ready, %u byte blocks, %lld bytes, %.3f MiB, %ls\n",
        diskletter, geo.Geometry.BytesPerSector, geo.DiskSize.QuadPart,
        toMiB(geo.DiskSize.QuadPart), name
    );

    /* if this is null it means this is a 'test drive letter only' 1 arg run */
    if(outfilepath == NULL)
    {
        myCloseHandle(diskhandle, "diskhandle");
        return 0;
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

    ret = docopy(diskhandle, isohandle, geo.Geometry.BytesPerSector, geo.DiskSize.QuadPart);
    myCloseHandle(diskhandle, "diskhandle");
    myCloseHandle(isohandle, "isohandle");
    return ret;
}

static int print_usage(const wchar_t * argv0)
{
    const wchar_t * fname = filepath_to_filename(argv0);
    fwprintf(stderr, L"%ls - rip a CD/DVD to an iso file\n", fname);
    fwprintf(stderr, L"Usage (rip): %ls diskletter isofile\n", fname);
    fwprintf(stderr, L"Usage (check drive): %ls diskletter\n", fname);
    fwprintf(stderr, L"Usage (check all drives): %ls all\n", fname);
    return 1;
}

static int listalldiscs(void)
{
    int c, zeros;

    zeros = 0;
    /* don't print 20+ times the bit not set in GetLogicalDrives() warning */
    for(c = 'A'; c <= 'Z'; ++c)
        if(isDiskAvailable(c))
            zeros += (0 == doit(c, NULL, stdout));

    /* return 1 if no good discs found and 0 if there are any */
    return zeros == 0;
}

int wmain(int argc, wchar_t ** argv)
{
    if(argc != 3 && argc != 2)
        return print_usage(argv[0]);

    if(argc == 2 && 0 == wcscmp(argv[1], L"all"))
        return listalldiscs();

    if(!isGoodDiskArg(argv[1]))
    {
        fwprintf(stderr, L"'%ls' is not a good disk letter (uppercase A-Z only, no : or / or \\)\n", argv[1]);
        return 1;
    }

    /* this access and char cast is safe after isGoodDiskArg returned true */
    return doit((char)argv[1][0], (argc == 3)?argv[2]:NULL, stderr);
}
