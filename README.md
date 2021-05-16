# bliso

Small standalone Windows only C command line program to rip raw data from a CD
or DVD or similar into an iso file (if the disc contains an ISO filesystem originally).

It tries to be friendly to user usage and script parsing, print good error
and usage messages, check the WinAPI calls, won't clobber output file, etc.

Written for personal use, for fun and to learn and experiment. If you use it be
careful and check that the rip is correct size, the ripped iso file mounts, etc.
I haven't done a lot of testing with real discs (just few dozen) or much research
on on-disc formats, this is a very basic tool.

Inspired by
[automatic-ripping-machine](https://github.com/automatic-ripping-machine/automatic-ripping-machine)
but much less featureful (raw rip only) and Windows only instead of Linux.

Licensed under **MIT License**.

Please let me know if you found it useful or ran into any bugs or any weird
behavior, I only tested it with my own external drive and a few discs so far.


# Usage

Call with just disk letter to check that drive. Call with disk letter and
filename to rip to that file. Call with just 'all' to check all drive letters.


# Download

Go to releases to download a Windows exe compiled with Pelles C with no `-O2`
to avoid running into any `-O2` optimizer bug similar to this one that affected
`stb_image`: [Pelles C forum bug report](https://forum.pellesc.de/index.php?topic=7837.0)


# Examples

```
$ bliso
bliso.exe - rip a CD/DVD to an iso file
Usage (rip): bliso.exe diskletter isofile
Usage (check drive): bliso.exe diskletter
Usage (check all drives): bliso.exe all
```

```
$ bliso all
Disc C: openDiskHandle returned INVALID_HANDLE_VALUE, GetLastError() = 5
Disc D: openDiskHandle returned INVALID_HANDLE_VALUE, GetLastError() = 5
Disc E: openDiskHandle returned INVALID_HANDLE_VALUE, GetLastError() = 5
Disc F: geo.Geometry.MediaType is 12 != RemovableMedia (11)
Disc G: DeviceIoControl returned false, GetLastError() == 21 == ERROR_NOT_READY, no disc in drive?
Disc H: ready, 2048 byte blocks, 253755392 bytes, 242.000 MiB, ReactOS
Disc I: ready, 2048 byte blocks, 417824768 bytes, 398.469 MiB, EICD1
```

```
$ bliso A
Disc A: bit not set in GetLogicalDrives()
```

```
$ bliso I file.iso
Disc I: ready, 2048 byte blocks, 417824768 bytes, 398.469 MiB, EICD1
file.iso - file already exists (GetLastError() == 80 == ERROR_FILE_EXISTS), refusing to clobber
```

```
$ bliso H
Disc H: ready, 2048 byte blocks, 253755392 bytes, 242.000 MiB, ReactOS

$ bliso H h.iso #H is an iso file mounted from a ram drive
Disc H: ready, 2048 byte blocks, 253755392 bytes, 242.000 MiB, ReactOS
PROGRESS: 16777216/253755392, 6.6%, 16.000 MiB, 0.0s, 1167.858 MiB/s
PROGRESS: 33554432/253755392, 13.2%, 32.000 MiB, 0.0s, 1287.416 MiB/s
//skipped for brevity
PROGRESS: 251658240/253755392, 99.2%, 240.000 MiB, 0.2s, 1438.401 MiB/s
PROGRESS: 253138944/253755392, 99.8%, 241.412 MiB, 0.2s, 1432.998 MiB/s
PADDING: from 253138944 to 253755392
PROGRESS: 253755392/253755392, 100.0%, 242.000 MiB, 0.2s, 1425.490 MiB/s
DONE!

$ sha1sum *.iso
87de1bf43a9aa5a2e4c4d5fe16ea13b1edc02502 *h.iso
87de1bf43a9aa5a2e4c4d5fe16ea13b1edc02502 *ReactOS-0.4.11-Live.iso
```

```
$ bliso I kingdoms.iso
Disc I: ready, 2048 byte blocks, 8268611584 bytes, 7885.563 MiB, kingdoms
PROGRESS: 16777216/8268611584, 0.2%, 16.000 MiB, 7.5s, 2.121 MiB/s
PROGRESS: 33554432/8268611584, 0.4%, 32.000 MiB, 16.3s, 1.960 MiB/s
//skipped for brevity
PROGRESS: 8254390272/8268611584, 99.8%, 7872.000 MiB, 1063.8s, 7.400 MiB/s
PROGRESS: 8268611584/8268611584, 100.0%, 7885.563 MiB, 1066.8s, 7.392 MiB/s
PROGRESS: 8268611584/8268611584, 100.0%, 7885.563 MiB, 1066.8s, 7.392 MiB/s
DONE!
```

```
$ bliso I edicd1.iso
Disc I: ready, 2048 byte blocks, 417824768 bytes, 398.469 MiB, EICD1
PROGRESS: 16777216/417824768, 4.0%, 16.000 MiB, 20.4s, 0.786 MiB/s
PROGRESS: 33554432/417824768, 8.0%, 32.000 MiB, 30.7s, 1.042 MiB/s
//skipped for brevity
PROGRESS: 417445888/417824768, 99.9%, 398.107 MiB, 197.2s, 2.019 MiB/s
PADDING: from 417445888 to 417824768
PROGRESS: 417824768/417824768, 100.0%, 398.469 MiB, 197.2s, 2.021 MiB/s
DONE!
```
