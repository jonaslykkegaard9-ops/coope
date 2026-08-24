<#
    shadowvolume.ps1 - the PowerShell companion to shadowvolume.c

    Same idea as the C component: open, read, enumerate and map files that live
    inside a Volume Shadow Copy (a VSS snapshot) named \Device\HarddiskVolumeShadowCopyN.
    A snapshot has no drive letter, so CreateFileW / Get-Item cannot reach it as it is.

    Two layers are shown here, matching the two honest answers to "can this be done
    in PowerShell":

      A. the easy way   - Win32_ShadowCopy for the device name + \\?\GLOBALROOT + .NET file IO.
      B. the faithful way - Add-Type P/Invoke into the exact same ntdll exports the C file binds.

    Dot-source it to get the $shadowvolume object without running the demo:
        . .\shadowvolume.ps1
    Run it directly (elevated) to run the demo:
        .\shadowvolume.ps1
#>

Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
#  B. The native surface, declared once.
#
#  [DllImport("ntdll.dll")] is the PowerShell equivalent of the C file's
#  GetModuleHandleW("ntdll.dll") + GetProcAddress binding - the CLR resolves the
#  exports itself, so there is no bind_ntdll() scaffolding to write.
# ---------------------------------------------------------------------------
if( -not ( 'Nt.Native' -as [type] ) ){
Add-Type -Namespace Nt -Name Native -PassThru -MemberDefinition @'
    // the nt types are not in a normal reference assembly, so declare them,
    // same as the C file declares them when you do not include <winternl.h>.
    [StructLayout(LayoutKind.Sequential)]
    public struct UNICODE_STRING {
        public ushort Length;           // in bytes, without the terminator
        public ushort MaximumLength;    // in bytes, room for the terminator
        public IntPtr Buffer;           // a pinned wchar_t*
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct OBJECT_ATTRIBUTES {
        public int    Length;
        public IntPtr RootDirectory;
        public IntPtr ObjectName;       // UNICODE_STRING*
        public uint   Attributes;
        public IntPtr SecurityDescriptor;
        public IntPtr SecurityQualityOfService;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct IO_STATUS_BLOCK {
        public IntPtr Status;
        public IntPtr Information;       // bytes transferred for read/write
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct OBJECT_DIRECTORY_INFORMATION {
        public UNICODE_STRING Name;
        public UNICODE_STRING TypeName;
    }

    [DllImport("ntdll.dll")] public static extern int NtOpenFile(
        out IntPtr handle, uint access, ref OBJECT_ATTRIBUTES attributes,
        out IO_STATUS_BLOCK iostatus, uint share, uint options );

    [DllImport("ntdll.dll")] public static extern int NtCreateFile(
        out IntPtr handle, uint access, ref OBJECT_ATTRIBUTES attributes,
        out IO_STATUS_BLOCK iostatus, IntPtr allocationsize, uint fileattributes,
        uint share, uint disposition, uint options, IntPtr ea, uint ealength );

    [DllImport("ntdll.dll")] public static extern int NtReadFile(
        IntPtr handle, IntPtr evt, IntPtr apc, IntPtr apccontext,
        out IO_STATUS_BLOCK iostatus, byte[] buffer, uint length,
        ref long offset, IntPtr key );

    [DllImport("ntdll.dll")] public static extern int NtClose( IntPtr handle );

    [DllImport("ntdll.dll")] public static extern int NtCreateSection(
        out IntPtr section, uint access, IntPtr attributes, ref long maximumsize,
        uint protect, uint allocationattributes, IntPtr file );

    [DllImport("ntdll.dll")] public static extern int NtMapViewOfSection(
        IntPtr section, IntPtr process, ref IntPtr baseaddress, IntPtr zerobits,
        IntPtr commitsize, IntPtr sectionoffset, ref IntPtr viewsize,
        uint inherit, uint allocationtype, uint protect );

    [DllImport("ntdll.dll")] public static extern int NtFlushVirtualMemory(
        IntPtr process, ref IntPtr baseaddress, ref IntPtr size, out IO_STATUS_BLOCK iostatus );

    [DllImport("ntdll.dll")] public static extern int NtUnmapViewOfSection(
        IntPtr process, IntPtr baseaddress );

    [DllImport("ntdll.dll")] public static extern int NtOpenDirectoryObject(
        out IntPtr handle, uint access, ref OBJECT_ATTRIBUTES attributes );

    [DllImport("ntdll.dll")] public static extern int NtQueryDirectoryObject(
        IntPtr handle, IntPtr buffer, uint length, byte singleentry, byte restart,
        ref uint context, out uint returnlength );

    [DllImport("ntdll.dll")] public static extern int NtQueryDirectoryFile(
        IntPtr handle, IntPtr evt, IntPtr apc, IntPtr apccontext,
        out IO_STATUS_BLOCK iostatus, IntPtr buffer, uint length, uint infoclass,
        byte singleentry, ref UNICODE_STRING mask, byte restart );

    [DllImport("ntdll.dll")] public static extern int RtlAdjustPrivilege(
        uint privilege, byte enable, byte thisthreadonly, out byte wasenabled );
'@ | Out-Null
}

# NT flags / constants - the same values shadowvolume.c defines.
$NT = @{
    OBJ_CASE_INSENSITIVE           = 0x00000040
    MAXIMUM_ALLOWED                = 0x02000000
    SYNCHRONIZE                    = 0x00100000
    FILE_LIST_DIRECTORY            = 0x00000001
    FILE_SHARE_READ                = 0x00000001
    FILE_SHARE_WRITE               = 0x00000002
    FILE_SHARE_DELETE              = 0x00000004
    FILE_ATTRIBUTE_NORMAL          = 0x00000080
    FILE_OVERWRITE_IF              = 0x00000005
    FILE_DIRECTORY_FILE            = 0x00000001
    FILE_SYNCHRONOUS_IO_NONALERT   = 0x00000020
    FILE_NON_DIRECTORY_FILE        = 0x00000040
    FILE_OPEN_FOR_BACKUP_INTENT    = 0x00004000
    DIRECTORY_QUERY                = 0x00000001
    FILE_DIRECTORY_INFORMATION     = 1
    SEC_COMMIT                     = 0x08000000
    PAGE_READWRITE                 = 0x00000004
    SECTION_MAP_READ               = 0x00000004
    SECTION_MAP_WRITE              = 0x00000002
    VIEW_UNMAP                     = 2
    SE_BACKUP_PRIVILEGE            = 17
    # NTSTATUS values are unsigned; store them as the signed int32 the DllImports return
    # (a plain [int]0x8000001A would overflow, the hex parses as a long that will not cast).
    STATUS_NO_MORE_ENTRIES         = [System.BitConverter]::ToInt32( [System.BitConverter]::GetBytes( [uint32] 0x8000001A ), 0 )
    STATUS_INVALID_PARAMETER       = [System.BitConverter]::ToInt32( [System.BitConverter]::GetBytes( [uint32] 0xC000000D ), 0 )
}
$NtCurrentProcess = [IntPtr]-1

# ---------------------------------------------------------------------------
#  The interface.
#
#  The C file is a struct of function pointers filled in at the bottom of the
#  file; the natural PowerShell parallel is a [pscustomobject] whose members are
#  ScriptMethods, assembled here in one place. Same shape, same verbs.
# ---------------------------------------------------------------------------
$script:LastStatus = 0

# build "\Device\HarddiskVolumeShadowCopyN" + "\some\file" into a pinned UNICODE_STRING.
# UNICODE_STRING carries a byte Length, so it never depends on a terminator - but we keep
# the buffer zero terminated so it stays printable, exactly like nt_path() in the C file.
function New-NtPath {
    param( [string] $Device, [string] $Path )
    $device = if( $Device ){ $Device } else { '' }
    $path   = if( $Path )   { $Path }   else { '' }
    $separator = ( $path.Length -ne 0 ) -and ( $path[0] -ne '\' ) -and ( $device[ $device.Length - 1 ] -ne '\' )
    $joined = $device + $( if( $separator ){ '\' } else { '' } ) + $path
    if( ( $device.Length -eq 0 ) -or ( $joined.Length * 2 -gt 0xFFFE ) ){ return $null }
    $buffer = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni( $joined )   # wchar_t*, zero terminated
    $us = New-Object Nt.Native+UNICODE_STRING
    $us.Length        = [ushort]( $joined.Length * 2 )
    $us.MaximumLength = [ushort]( ( $joined.Length + 1 ) * 2 )
    $us.Buffer        = $buffer
    # pin the UNICODE_STRING itself so we can pass its address as ObjectName
    $us_native = [System.Runtime.InteropServices.Marshal]::AllocHGlobal( [System.Runtime.InteropServices.Marshal]::SizeOf( $us ) )
    [System.Runtime.InteropServices.Marshal]::StructureToPtr( $us, $us_native, $false )
    [pscustomobject]@{ Struct = $us; Native = $us_native; Buffer = $buffer }
}
function Free-NtPath {
    param( $NtPath )
    if( $NtPath.Native ){ [System.Runtime.InteropServices.Marshal]::FreeHGlobal( $NtPath.Native ) }
    if( $NtPath.Buffer ){ [System.Runtime.InteropServices.Marshal]::FreeHGlobal( $NtPath.Buffer ) }
}

function New-ObjectAttributes {
    param( [IntPtr] $ObjectName )
    $oa = New-Object Nt.Native+OBJECT_ATTRIBUTES
    $oa.Length     = [System.Runtime.InteropServices.Marshal]::SizeOf( $oa )
    $oa.ObjectName = $ObjectName
    $oa.Attributes = $NT.OBJ_CASE_INSENSITIVE
    $oa
}

# open an nt path with MAXIMUM_ALLOWED, the object manager hands back every right the
# token is allowed on it - read on a read-only snapshot, everything on a writable volume.
function Invoke-NtOpen {
    param( [string] $Device, [string] $Path, [uint32] $Options )
    $ntpath = New-NtPath -Device $Device -Path $Path
    if( $null -eq $ntpath ){ $script:LastStatus = $NT.STATUS_INVALID_PARAMETER; return [IntPtr]::Zero }
    try {
        $oa = New-ObjectAttributes -ObjectName $ntpath.Native
        $iostatus = New-Object Nt.Native+IO_STATUS_BLOCK
        $handle = [IntPtr]::Zero
        $script:LastStatus = [Nt.Native]::NtOpenFile(
            [ref] $handle,
            [uint32]( $NT.MAXIMUM_ALLOWED -bor $NT.SYNCHRONIZE ),
            [ref] $oa, [ref] $iostatus,
            [uint32]( $NT.FILE_SHARE_READ -bor $NT.FILE_SHARE_WRITE -bor $NT.FILE_SHARE_DELETE ),
            $Options )
        if( $script:LastStatus -ge 0 ){ $handle } else { [IntPtr]::Zero }
    } finally { Free-NtPath $ntpath }
}

$shadowvolume = [pscustomobject]@{}

# open( device, path ) - the file inside the snapshot, opened for backup intent.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Open -Value {
    param( [string] $Device, [string] $Path )
    Invoke-NtOpen -Device $Device -Path $Path -Options ( $NT.FILE_SYNCHRONOUS_IO_NONALERT -bor $NT.FILE_NON_DIRECTORY_FILE -bor $NT.FILE_OPEN_FOR_BACKUP_INTENT )
}

# read( file, bytes, offset ) - NtReadFile with an explicit offset, no file-pointer dependency.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Read -Value {
    param( [IntPtr] $File, [int] $Bytes, [long] $Offset )
    $buffer = New-Object byte[] $Bytes
    $iostatus = New-Object Nt.Native+IO_STATUS_BLOCK
    $position = $Offset
    $script:LastStatus = [Nt.Native]::NtReadFile( $File, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref] $iostatus, $buffer, [uint32] $Bytes, [ref] $position, [IntPtr]::Zero )
    if( $script:LastStatus -ge 0 ){
        $read = [int] $iostatus.Information
        if( $read -lt $Bytes ){ $buffer[0..($read-1)] } else { $buffer }
    } else { $null }
}

# close( file )
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Close -Value {
    param( [IntPtr] $File )
    if( $File -ne [IntPtr]::Zero ){ [void] [Nt.Native]::NtClose( $File ) }
}

# for_each_device() - walk \Device, return every HarddiskVolumeShadowCopyN present.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name ForEachDevice -Value {
    $ntpath = New-NtPath -Device '\Device' -Path ''
    $devices = New-Object System.Collections.Generic.List[string]
    try {
        $oa = New-ObjectAttributes -ObjectName $ntpath.Native
        $directory = [IntPtr]::Zero
        $script:LastStatus = [Nt.Native]::NtOpenDirectoryObject( [ref] $directory, [uint32] $NT.DIRECTORY_QUERY, [ref] $oa )
        if( $script:LastStatus -lt 0 ){ return $devices }   # querying \Device needs an elevated token
        $size = 2048
        $buffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal( $size )
        try {
            $context = [uint32] 0
            $first = $true
            while( $true ){
                $returned = [uint32] 0
                $script:LastStatus = [Nt.Native]::NtQueryDirectoryObject( $directory, $buffer, [uint32] $size, 1, $( if( $first ){ 1 } else { 0 } ), [ref] $context, [ref] $returned )
                if( $script:LastStatus -lt 0 ){ break }
                $first = $false
                $entry = [System.Runtime.InteropServices.Marshal]::PtrToStructure( $buffer, [type] 'Nt.Native+OBJECT_DIRECTORY_INFORMATION' )
                if( $entry.Name.Buffer -eq [IntPtr]::Zero ){ break }
                $name = [System.Runtime.InteropServices.Marshal]::PtrToStringUni( $entry.Name.Buffer, $entry.Name.Length / 2 )
                if( $name -like 'HarddiskVolumeShadowCopy*' ){ $devices.Add( "\Device\$name" ) }
                if( $script:LastStatus -eq $NT.STATUS_NO_MORE_ENTRIES ){ break }
            }
        } finally { [System.Runtime.InteropServices.Marshal]::FreeHGlobal( $buffer ) }
        [void] [Nt.Native]::NtClose( $directory )
    } finally { Free-NtPath $ntpath }
    $devices
}

# enable_backup_privilege() - SeBackupPrivilege lets a backup operator read files the ACL denies.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name EnableBackupPrivilege -Value {
    $wasenabled = [byte] 0
    $script:LastStatus = [Nt.Native]::RtlAdjustPrivilege( [uint32] $NT.SE_BACKUP_PRIVILEGE, 1, 0, [ref] $wasenabled )
    $script:LastStatus -ge 0
}

# open_writable( ntpath ) - NtCreateFile, \??\C:\... create-or-truncate. A snapshot is read only,
# so this is for a normal file, the target the writable mapping needs.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name OpenWritable -Value {
    param( [string] $NtPath )
    $ntpath = New-NtPath -Device $NtPath -Path ''
    if( $null -eq $ntpath ){ $script:LastStatus = $NT.STATUS_INVALID_PARAMETER; return [IntPtr]::Zero }
    try {
        $oa = New-ObjectAttributes -ObjectName $ntpath.Native
        $iostatus = New-Object Nt.Native+IO_STATUS_BLOCK
        $file = [IntPtr]::Zero
        $script:LastStatus = [Nt.Native]::NtCreateFile(
            [ref] $file,
            [uint32]( $NT.MAXIMUM_ALLOWED -bor $NT.SYNCHRONIZE ),
            [ref] $oa, [ref] $iostatus,
            [IntPtr]::Zero,                     # the file grows to fit the section, no preallocation
            [uint32] $NT.FILE_ATTRIBUTE_NORMAL,
            [uint32] $NT.FILE_SHARE_READ,
            [uint32] $NT.FILE_OVERWRITE_IF,     # create it, or truncate one already there
            [uint32]( $NT.FILE_SYNCHRONOUS_IO_NONALERT -bor $NT.FILE_NON_DIRECTORY_FILE ),
            [IntPtr]::Zero, 0 )
        if( $script:LastStatus -ge 0 ){ $file } else { [IntPtr]::Zero }
    } finally { Free-NtPath $ntpath }
}

# map( file, size ) - NtCreateSection + NtMapViewOfSection, read/write. size 0 maps the whole
# existing file, otherwise that many bytes and the file is extended to fit.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Map -Value {
    param( [IntPtr] $File, [int] $Size )
    if( $File -eq [IntPtr]::Zero ){ $script:LastStatus = $NT.STATUS_INVALID_PARAMETER; return $null }
    $section = [IntPtr]::Zero
    $maximum = [long] $Size
    $script:LastStatus = [Nt.Native]::NtCreateSection(
        [ref] $section,
        [uint32]( $NT.SECTION_MAP_READ -bor $NT.SECTION_MAP_WRITE ),
        [IntPtr]::Zero, [ref] $maximum,
        [uint32] $NT.PAGE_READWRITE, [uint32] $NT.SEC_COMMIT, $File )
    if( $script:LastStatus -lt 0 ){ return $null }
    $base = [IntPtr]::Zero
    $viewsize = [IntPtr] $Size          # 0 => the whole section
    $script:LastStatus = [Nt.Native]::NtMapViewOfSection(
        $section, $NtCurrentProcess, [ref] $base, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero,
        [ref] $viewsize, [uint32] $NT.VIEW_UNMAP, 0, [uint32] $NT.PAGE_READWRITE )
    if( $script:LastStatus -lt 0 ){ [void] [Nt.Native]::NtClose( $section ); return $null }
    [pscustomobject]@{ Base = $base; Size = [int] $viewsize; Section = $section }
}

# write bytes into a mapped view - PowerShell reaches the view through Marshal.Copy,
# there is no raw pointer arithmetic like the C wmemcpy into view.base.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name WriteView -Value {
    param( $View, [byte[]] $Bytes )
    [System.Runtime.InteropServices.Marshal]::Copy( $Bytes, 0, $View.Base, [Math]::Min( $Bytes.Length, $View.Size ) )
}

# flush( view ) - NtFlushVirtualMemory pushes the dirty pages back to the file.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Flush -Value {
    param( $View )
    if( $null -eq $View ){ return $false }
    $base = $View.Base
    $region = [IntPtr] $View.Size
    $iostatus = New-Object Nt.Native+IO_STATUS_BLOCK
    $script:LastStatus = [Nt.Native]::NtFlushVirtualMemory( $NtCurrentProcess, [ref] $base, [ref] $region, [ref] $iostatus )
    $script:LastStatus -ge 0
}

# unmap( view ) - NtUnmapViewOfSection and close the section.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Unmap -Value {
    param( $View )
    if( $null -eq $View ){ return }
    if( $View.Base    -ne [IntPtr]::Zero ){ [void] [Nt.Native]::NtUnmapViewOfSection( $NtCurrentProcess, $View.Base ) }
    if( $View.Section -ne [IntPtr]::Zero ){ [void] [Nt.Native]::NtClose( $View.Section ) }
}

# find( device, filename ) - NtQueryDirectoryFile a single-entry mask in <device>\Windows;
# a hit returns the full nt path, ready to hand to run(). $null if it is not there.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name Find -Value {
    param( [string] $Device, [string] $Filename )
    $dir = New-NtPath -Device $Device -Path '\Windows'
    if( $null -eq $dir ){ $script:LastStatus = $NT.STATUS_INVALID_PARAMETER; return $null }
    $maskpath = $null
    try {
        $oa = New-ObjectAttributes -ObjectName $dir.Native
        $iostatus = New-Object Nt.Native+IO_STATUS_BLOCK
        $directory = [IntPtr]::Zero
        $script:LastStatus = [Nt.Native]::NtOpenFile(
            [ref] $directory,
            [uint32]( $NT.FILE_LIST_DIRECTORY -bor $NT.SYNCHRONIZE ),
            [ref] $oa, [ref] $iostatus,
            [uint32]( $NT.FILE_SHARE_READ -bor $NT.FILE_SHARE_WRITE -bor $NT.FILE_SHARE_DELETE ),
            [uint32]( $NT.FILE_SYNCHRONOUS_IO_NONALERT -bor $NT.FILE_DIRECTORY_FILE -bor $NT.FILE_OPEN_FOR_BACKUP_INTENT ) )
        if( $script:LastStatus -lt 0 ){ return $null }
        $maskbuffer = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni( $Filename )
        $mask = New-Object Nt.Native+UNICODE_STRING
        $mask.Length        = [ushort]( $Filename.Length * 2 )
        $mask.MaximumLength = [ushort]( ( $Filename.Length + 1 ) * 2 )
        $mask.Buffer        = $maskbuffer
        $entry = [System.Runtime.InteropServices.Marshal]::AllocHGlobal( 2048 )
        try {
            $script:LastStatus = [Nt.Native]::NtQueryDirectoryFile(
                $directory, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref] $iostatus,
                $entry, 2048, [uint32] $NT.FILE_DIRECTORY_INFORMATION, 1, [ref] $mask, 1 )
        } finally {
            [System.Runtime.InteropServices.Marshal]::FreeHGlobal( $entry )
            [System.Runtime.InteropServices.Marshal]::FreeHGlobal( $maskbuffer )
        }
        [void] [Nt.Native]::NtClose( $directory )
        if( $script:LastStatus -lt 0 ){ return $null }   # STATUS_NO_SUCH_FILE when it is not present
        "$Device\Windows\$Filename"
    } finally { Free-NtPath $dir }
}

# last_status() - the NTSTATUS of the last call, negative means failure.
$shadowvolume | Add-Member -MemberType ScriptMethod -Name LastStatus -Value { $script:LastStatus }

# ---------------------------------------------------------------------------
#  A. The easy way, for contrast: no P/Invoke at all.
#  Win32_ShadowCopy gives the device object, \\?\GLOBALROOT lets .NET reach it -
#  this is the \\?\GLOBALROOT escape the C article deliberately avoids.
# ---------------------------------------------------------------------------
$shadowvolume | Add-Member -MemberType ScriptMethod -Name ListShadowCopiesWmi -Value {
    Get-CimInstance -ClassName Win32_ShadowCopy | Select-Object -ExpandProperty DeviceObject
}
$shadowvolume | Add-Member -MemberType ScriptMethod -Name ReadViaGlobalRoot -Value {
    param( [string] $DeviceObject, [string] $RelativePath )
    # $DeviceObject is \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopyN
    [System.IO.File]::ReadAllBytes( ( Join-Path $DeviceObject $RelativePath ) )
}

# ---------------------------------------------------------------------------
#  The demo - the PowerShell analog of the C file's __INCLUDE_LEVEL__ == 0 /
#  -DSHADOWVOLUME_MAIN block. It runs only when the script is invoked directly,
#  not when it is dot-sourced (InvocationName is '.' for a dot-source).
# ---------------------------------------------------------------------------
if( $MyInvocation.InvocationName -ne '.' ){
    [void] $shadowvolume.EnableBackupPrivilege()

    $devices = $shadowvolume.ForEachDevice()
    if( $devices.Count -eq 0 ){
        Write-Host ( "no shadow copies present (status {0:X8}), create one first" -f $shadowvolume.LastStatus() )
    } else {
        $devices | ForEach-Object { Write-Host "found $_" }

        # the event log is held open by the event log service; the snapshot is a point in
        # time copy, so this read never contends with it.
        $file = $shadowvolume.Open( '\Device\HarddiskVolumeShadowCopy1', '\Windows\System32\winevt\Logs\System.evtx' )
        if( $file -eq [IntPtr]::Zero ){
            Write-Host ( "open failed with status {0:X8}" -f $shadowvolume.LastStatus() )
        } else {
            $header = $shadowvolume.Read( $file, 8, 0 )
            if( $header ){ Write-Host ( "read {0} bytes from the snapshot: {1}" -f $header.Length, [System.Text.Encoding]::ASCII.GetString( $header ) ) }  # "ElfFile"
            $shadowvolume.Close( $file )
        }
    }

    # Memory map a scratch file under %TEMP% and change its bytes to %comspec%.
    # \??\ is the nt symlink onto the win32 drive letters.
    $comspec  = [System.Environment]::ExpandEnvironmentVariables( '%comspec%' )
    $ntpath   = '\??\' + [System.Environment]::ExpandEnvironmentVariables( '%TEMP%\comspec.bin' )
    $bytes    = [System.Text.Encoding]::Unicode.GetBytes( $comspec + "`0" )

    $scratch = $shadowvolume.OpenWritable( $ntpath )
    if( $scratch -ne [IntPtr]::Zero ){
        $view = $shadowvolume.Map( $scratch, $bytes.Length )     # the section extends the file to fit
        if( $view ){
            $shadowvolume.WriteView( $view, $bytes )
            [void] $shadowvolume.Flush( $view )
            $shadowvolume.Unmap( $view )
            Write-Host ( "wrote {0} bytes of %comspec% ({1}) into {2}" -f $bytes.Length, $comspec, $ntpath )
        } else {
            Write-Host ( "map failed with status {0:X8}" -f $shadowvolume.LastStatus() )
        }
        $shadowvolume.Close( $scratch )
    } else {
        Write-Host ( "open_writable failed with status {0:X8}" -f $shadowvolume.LastStatus() )
    }

    # Find hh.exe inside the snapshot and open it with maximum access. The article shows
    # how run() would section and launch that copy with RtlCreateUserProcess.
    $hhpath = $shadowvolume.Find( '\Device\HarddiskVolumeShadowCopy1', 'hh.exe' )
    if( $hhpath ){
        Write-Host "found $hhpath in the snapshot"
    } else {
        Write-Host ( "hh.exe not found in the snapshot (status {0:X8})" -f $shadowvolume.LastStatus() )
    }
}
