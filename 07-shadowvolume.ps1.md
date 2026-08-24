# Can the shadow volume code be done in PowerShell?

A fair question to ask about `shadowvolume.c` - it opens files inside a Volume Shadow Copy through the native NT API, does any of that actually need a compiled C program? The answer is no: PowerShell reaches every one of those calls. `shadowvolume.ps1` is the companion that shows it, and it turns out there are two honest answers.

## The easy way: GLOBALROOT + WMI

Most of the C example exists to solve one problem - a shadow copy is a device object named `\Device\HarddiskVolumeShadowCopyN` with no drive letter, so `CreateFileW` cannot reach it. The C article mentions the Win32 escape hatch and then deliberately walks past it. PowerShell can just take it:

```powershell
$sc = Get-CimInstance -ClassName Win32_ShadowCopy | Select-Object -First 1
# $sc.DeviceObject is \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy1
$bytes = [System.IO.File]::ReadAllBytes( "$($sc.DeviceObject)\Windows\System32\winevt\Logs\System.evtx" )
```

`Win32_ShadowCopy` hands you the device name so you never guess `N`, and `\\?\GLOBALROOT` tells the Win32 layer to stop rewriting the path and pass it to the object manager verbatim. `[System.IO.File]` then reads it like any other file. Three lines, no P/Invoke.

That is the whole point the C article was making, from the other side: the `\\?\GLOBALROOT` prefix works *because* it drops down to the object manager - it is the same NT name, wrapped so the Win32 layer will carry it. PowerShell gets it for free because it stands on top of the Win32 layer. It also means you never left that layer, which is exactly what the C example set out to avoid.

Enabling `SeBackupPrivilege` - what lets you read another user's files inside the snapshot - is the one piece the easy way cannot do on its own; there is no Win32 cmdlet for it, so even the easy path drops to the native `RtlAdjustPrivilege`. Which leads to the second answer.

## The faithful way: P/Invoke into ntdll

To actually do what the C file does - name the object directly, no GLOBALROOT wrapper - PowerShell calls the same ntdll exports. You declare them once with `Add-Type`:

```powershell
Add-Type -Namespace Nt -Name Native -MemberDefinition @'
    [DllImport("ntdll.dll")] public static extern int NtOpenFile(
        out IntPtr handle, uint access, ref OBJECT_ATTRIBUTES attributes,
        out IO_STATUS_BLOCK iostatus, uint share, uint options );
    // ... NtReadFile, NtClose, NtQueryDirectoryObject, RtlAdjustPrivilege, NtCreateSection, ...
'@
```

Here is a genuine win over the C version. `shadowvolume.c` cannot link an import library for these, so it resolves them by hand:

```c
const HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );
#define bind(function) ( nt.function = (typeof(nt.function))GetProcAddress( ntdll, #function ) )
```

`[DllImport("ntdll.dll")]` *is* that binding. The CLR resolves the export the first time the method is called, so the whole `bind_ntdll()` scaffolding - the function-pointer struct, the `GetProcAddress` loop, the "is it bound yet" guard - simply does not exist in the PowerShell version. You name the function once, in the `DllImport`, and call it.

## The interface

The C file is a `struct shadowvolume` of function pointers, filled in at the bottom of the file so the private implementation stays private. The natural PowerShell parallel is a `[pscustomobject]` whose members are `ScriptMethod`s, assembled in one place:

```powershell
$shadowvolume = [pscustomobject]@{}
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Open -Value { param($Device,$Path) ... }
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Read -Value { param($File,$Bytes,$Offset) ... }
# Close, ForEachDevice, EnableBackupPrivilege, OpenWritable, Map, Flush, Unmap, Find, LastStatus
```

Same shape, same verbs - a value you pass around whose methods are the only way in. The vtable the C file fills at link time becomes a bag of script blocks you attach at load time.

## The types

`NtOpenFile` needs `UNICODE_STRING`, `OBJECT_ATTRIBUTES` and `IO_STATUS_BLOCK`, and they are not in any reference assembly, so - exactly like the C file declaring them when you do not include `<winternl.h>` - you declare them in the `Add-Type` block:

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct UNICODE_STRING {
    public ushort Length;           // in bytes, without the terminator
    public ushort MaximumLength;
    public IntPtr Buffer;           // a pinned wchar_t*
}
```

`[StructLayout(LayoutKind.Sequential)]` is the C# way to say "lay these fields out in order, like a C struct". The catch PowerShell adds is that you manage the memory yourself: the NT path has to be a real `wchar_t*` that stays put across the call, so you pin it:

```powershell
$buffer = [Runtime.InteropServices.Marshal]::StringToHGlobalUni( $joined )   # wchar_t*, zero terminated
$us.Length = [ushort]( $joined.Length * 2 )                                   # bytes, like the C USHORT
```

That byte `Length` is the same detail the C article calls out - it is a `USHORT`, so a path over `0xFFFE` bytes is refused - and the PowerShell builder refuses it for the same reason.

## Opening and reading

Once the types are in place the call is a transcription of the C. Same access mask, same flags:

```powershell
[Nt.Native]::NtOpenFile(
    [ref] $handle,
    $NT.MAXIMUM_ALLOWED -bor $NT.SYNCHRONIZE,
    [ref] $oa, [ref] $iostatus,
    $NT.FILE_SHARE_READ -bor $NT.FILE_SHARE_WRITE -bor $NT.FILE_SHARE_DELETE,
    $NT.FILE_SYNCHRONOUS_IO_NONALERT -bor $NT.FILE_NON_DIRECTORY_FILE -bor $NT.FILE_OPEN_FOR_BACKUP_INTENT )
```

`MAXIMUM_ALLOWED` hands back read on a read-only snapshot and everything on a writable volume, `FILE_OPEN_FOR_BACKUP_INTENT` paired with the backup privilege reads past the ACL, and `FILE_SYNCHRONOUS_IO_NONALERT` gives the handle its own file pointer so `NtReadFile` blocks - identical to the C, because it is the same function.

## Mapping and find

`Map` is `NtCreateSection` + `NtMapViewOfSection` with `PAGE_READWRITE` and `SEC_COMMIT`, and passing a size larger than the file extends it to fit, same as the C. The one PowerShell difference is how you touch the mapped view: C writes straight into `view.base` with `wmemcpy`, PowerShell has no raw pointer, so it copies through the marshaller:

```powershell
[System.Runtime.InteropServices.Marshal]::Copy( $bytes, 0, $view.Base, $view.Size )
```

`Find` opens `<device>\Windows` as a directory and calls `NtQueryDirectoryFile` with the filename as a single-entry mask - a hit means the file is there, and it returns the full NT path.

`Run` is the last verb in the C file - `RtlCreateProcessParametersEx` + `RtlCreateUserProcess` + `NtResumeThread` to launch the copy that lives inside the snapshot. It is reachable from PowerShell the same way, but the marshalling is heavy: `RTL_USER_PROCESS_INFORMATION` with its embedded `SECTION_IMAGE_INFORMATION`, and process parameters that have to outlive the call. The shape is:

```powershell
[Nt.Native]::RtlCreateProcessParametersEx( [ref] $parameters, [ref] $image, ... )
[Nt.Native]::RtlCreateUserProcess( [ref] $image, $OBJ_CASE_INSENSITIVE, $parameters, ..., [ref] $information )
[Nt.Native]::NtResumeThread( $information.ThreadHandle, [IntPtr]::Zero )
```

`shadowvolume.ps1` ports the core faithfully and stops `run` at this sketch - the point of the exercise is that every call is reachable, and by `find` that point is made.

## Trying it

Same as the C demo: run it elevated (enumerating `\Device` and the backup privilege both need it), and create a snapshot first if you have none:

```powershell
(Get-WmiObject -List Win32_ShadowCopy).Create("C:\", "ClientAccessible")
.\shadowvolume.ps1
```

It enables the backup privilege, lists the shadow copy devices, reads the 8-byte `System.evtx` header - which prints `ElfFile`, the event log magic - maps a scratch file under `%TEMP%` and writes `%comspec%` into it, then locates `hh.exe` in the snapshot. Dot-source it instead (`. .\shadowvolume.ps1`) and the demo does not run - you just get the `$shadowvolume` object - which is the PowerShell version of the C file's `__INCLUDE_LEVEL__ == 0` / `-DSHADOWVOLUME_MAIN` split between "included as a component" and "compiled as a program".

## What PowerShell costs you

So, yes - all of it can be done in PowerShell, and the easy path is genuinely three lines. The faithful port is not three lines, and it is worth being honest about why:

- **Manual memory.** Every `UNICODE_STRING` buffer is an `AllocHGlobal` / `StringToHGlobalUni` you have to free. C's `wchar_t buffer[1024]` on the stack becomes a heap allocation with a `finally` to release it.
- **No `typeof` trick.** The C file writes each pointer type exactly once and reuses it with `typeof(nt.function)`; the PowerShell version spells out every `DllImport` signature by hand.
- **A compile on first run.** `Add-Type` compiles the C# block the first time the script loads - a one-time cost the C binary paid at build time.
- **Marshalling in the way.** No raw pointers means `Marshal.Copy` and `PtrToStructure` everywhere the C just dereferences.

But under all of it, the calls are byte-for-byte the same `NtOpenFile`, `NtReadFile` and `RtlAdjustPrivilege`. The NT API does not know or care that a script is calling it - which was the thing worth finding out.
