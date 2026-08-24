# Opening a file on a shadow volume with the NT API

`shadowvolume.c` is a small component in the same style as the rest of this repository - a public interface at the top of the file, the private implementation below the `__INCLUDE_LEVEL__ == 0` guard. It opens and reads a file that lives inside a Volume Shadow Copy (a VSS snapshot) by talking to the native NT API directly instead of the Win32 layer.

## Why the NT API and not `CreateFile`

A shadow copy is published by the `volsnap` driver as a device object named:

```
\Device\HarddiskVolumeShadowCopyN
```

It has no drive letter and no DOS device name, so `CreateFileW` cannot reach it as it is - the closest you can get from Win32 is the `\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopyN\...` escape hatch, which just tells the Win32 layer to stop rewriting the path and hand it to the object manager verbatim.

The NT API names kernel objects directly, so there is nothing to escape. `NtOpenFile` takes an `OBJECT_ATTRIBUTES` whose `ObjectName` is a full NT path, and the device path plus the in-volume file path *is* that name:

```
\Device\HarddiskVolumeShadowCopy1\Windows\System32\winevt\Logs\System.evtx
```

That is the whole idea of the example - the snapshot device and the file inside it are joined into one `UNICODE_STRING` and opened in a single call.

## The interface

```c
extern const struct shadowvolume{
    HANDLE   (*open)( const wchar_t* device, const wchar_t* path );
    uint32_t (*read)( HANDLE file, void* buffer, uint32_t bytes, uint64_t offset );
    void     (*close)( HANDLE file );
    uint32_t (*for_each_device)( void(*found)( const wchar_t* device ) );
    bool     (*enable_backup_privilege)( void );
    long     (*last_status)( void );
}shadowvolume;
```

- `open` joins `device` and `path` and calls `NtOpenFile`.
- `read` is `NtReadFile` with an explicit byte offset, so the caller does not depend on a file pointer.
- `for_each_device` enumerates `\Device` through `NtQueryDirectoryObject` and reports every `HarddiskVolumeShadowCopyN` that currently exists, so you do not have to guess `N`.
- `enable_backup_privilege` turns on `SeBackupPrivilege` in the current token, which is what lets you read another user's files inside the snapshot.
- `last_status` returns the `NTSTATUS` of the previous call - negative is failure.

## Binding ntdll at runtime

The native functions are not in an import library you would normally link, so the component resolves them from the already-mapped `ntdll.dll` on first use:

```c
const HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );  // always loaded, never LoadLibrary
#define bind(function) ( nt.function = (typeof(nt.function))GetProcAddress( ntdll, #function ) )
return  bind(NtOpenFile) && bind(NtReadFile) && bind(NtClose) &&
        bind(NtOpenDirectoryObject) && bind(NtQueryDirectoryObject) && bind(RtlAdjustPrivilege);
```

`typeof(nt.function)` reuses the field's own type as the cast, so the pointer type is written exactly once - in the struct - the same trick used elsewhere in this repo with `typeof(console)` and `typeof(timers)`.

## The types

If you build against `<winternl.h>` or the WDK, the NT structures already exist. To keep the example self contained it declares `UNICODE_STRING`, `OBJECT_ATTRIBUTES` and `IO_STATUS_BLOCK` itself, guarded by `!defined(_WINTERNL_) && !defined(_NTDEF_)` so it does not clash if you do pull those headers in.

`UNICODE_STRING` carries an explicit byte `Length`, so it never depends on a terminator - `nt_path` fills that in while still keeping the buffer zero terminated so it stays printable. Note `Length` is a `USHORT`, which is why the builder refuses a path over `0xFFFE` bytes.

## Opening

```c
OBJECT_ATTRIBUTES attributes = {
    .Length     = sizeof(attributes),
    .ObjectName = &objectname,
    .Attributes = OBJ_CASE_INSENSITIVE
};
status = nt.NtOpenFile(
    &file,
    FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
    &attributes,
    &iostatus,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT
);
```

- `FILE_SYNCHRONOUS_IO_NONALERT` gives the handle its own file pointer and makes `NtReadFile` block, so the code stays straight-line.
- `FILE_OPEN_FOR_BACKUP_INTENT` is the native form of `FILE_FLAG_BACKUP_SEMANTICS`; paired with `SeBackupPrivilege` it lets a backup operator read files the ACL would otherwise deny.
- A shadow copy is read only, so only read access and no write disposition is requested.

## Trying it

```
clang -DSHADOWVOLUME_MAIN shadowvolume.c -o shadowvolume.exe
```

Run it from an elevated prompt (enumerating `\Device` and using the backup privilege both need it). Create a snapshot first if you have none, e.g. from an admin PowerShell:

```
(Get-WmiObject -List Win32_ShadowCopy).Create("C:\", "ClientAccessible")
```

The demo enables the backup privilege, lists the shadow copy devices, opens `System.evtx` inside the first snapshot and prints its 8 byte header - which reads `ElfFile`, the event log magic - to prove the read came from inside the snapshot.
