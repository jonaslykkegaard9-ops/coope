#include "stdafx.h"
#ifndef shadowvolume
#	define shadowvolume shadowvolume
	/*	A shadow copy is exposed by volsnap as a device object named
		\Device\HarddiskVolumeShadowCopyN - it has no drive letter and no dos device name,
		so it is not reachable through the win32 namespace without the \\?\GLOBALROOT escape.
		The nt api names objects directly, so we can just open the device relative path as it is. */
	extern const struct shadowvolume{
		/*	device is the shadow copy device, path is the file inside it:
			shadowvolume.open( L"\\Device\\HarddiskVolumeShadowCopy1", L"\\Windows\\System32\\winevt\\Logs\\System.evtx" ) */
		HANDLE(*open)( const wchar_t* device, const wchar_t* path );
		uint32_t(*read)( HANDLE file, void* buffer, uint32_t bytes, uint64_t offset );
		void(*close)( HANDLE file );
		/*	walks \Device and reports every HarddiskVolumeShadowCopyN currently present, returns the count */
		uint32_t(*for_each_device)( void(*found)( const wchar_t* device ) );
		/*	reading other users files inside the snapshot needs SeBackupPrivilege enabled in the token */
		bool(*enable_backup_privilege)( void );
		/*	the NTSTATUS of the last call, negative means failure */
		long(*last_status)( void );
	}shadowvolume;
#	if __INCLUDE_LEVEL__ == 0
		/*	the nt types are not in Windows.h - if you include <winternl.h> or the wdk headers
			instead, drop this block, the guards below detect both of them. */
#		if !defined(_WINTERNL_) && !defined(_NTDEF_)
			typedef struct _UNICODE_STRING{
				USHORT Length;			/* in bytes, not characters, and without the terminator */
				USHORT MaximumLength;	/* in bytes, including room for a terminator */
				PWSTR  Buffer;
			}UNICODE_STRING, *PUNICODE_STRING;
			typedef struct _OBJECT_ATTRIBUTES{
				ULONG			Length;
				HANDLE			RootDirectory;
				PUNICODE_STRING	ObjectName;
				ULONG			Attributes;
				PVOID			SecurityDescriptor;
				PVOID			SecurityQualityOfService;
			}OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
			typedef struct _IO_STATUS_BLOCK{
				union{
					NTSTATUS	Status;
					PVOID		Pointer;
				};
				ULONG_PTR		Information;	/* bytes transferred for read/write */
			}IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
#		endif
		typedef struct _OBJECT_DIRECTORY_INFORMATION{
			UNICODE_STRING Name;
			UNICODE_STRING TypeName;
		}OBJECT_DIRECTORY_INFORMATION;

#		define OBJ_CASE_INSENSITIVE			0x00000040ul
#		define FILE_OPEN					0x00000001ul	/* CreateDisposition: fail if it is not there */
#		define FILE_SYNCHRONOUS_IO_NONALERT	0x00000020ul	/* the handle keeps its own file pointer, NtReadFile blocks */
#		define FILE_NON_DIRECTORY_FILE		0x00000040ul
#		define FILE_OPEN_FOR_BACKUP_INTENT	0x00004000ul	/* pairs with SeBackupPrivilege, like FILE_FLAG_BACKUP_SEMANTICS */
#		define DIRECTORY_QUERY				0x00000001ul
#		define STATUS_NO_MORE_ENTRIES		0x8000001Al
#		define STATUS_INVALID_PARAMETER		0xC000000Dl
#		define SE_BACKUP_PRIVILEGE			17ul

		static struct{
			NTSTATUS(NTAPI* NtOpenFile)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, IO_STATUS_BLOCK*, ULONG share, ULONG options );
			NTSTATUS(NTAPI* NtReadFile)( HANDLE, HANDLE event, PVOID apc, PVOID apccontext, IO_STATUS_BLOCK*, PVOID buffer, ULONG length, LARGE_INTEGER* offset, ULONG* key );
			NTSTATUS(NTAPI* NtClose)( HANDLE );
			NTSTATUS(NTAPI* NtOpenDirectoryObject)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES* );
			NTSTATUS(NTAPI* NtQueryDirectoryObject)( HANDLE, PVOID buffer, ULONG length, BOOLEAN singleentry, BOOLEAN restart, ULONG* context, ULONG* returnlength );
			NTSTATUS(NTAPI* RtlAdjustPrivilege)( ULONG privilege, BOOLEAN enable, BOOLEAN thisthreadonly, BOOLEAN* wasenabled );
		}nt;
		static NTSTATUS status;

		static bool bind_ntdll( void ){
			if( nt.NtOpenFile ){ return true; }
			const HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );	/* always mapped, never needs LoadLibrary */
			if( ntdll == 0 ){ return false; }
#			define bind(function) ( nt.function = (typeof(nt.function))GetProcAddress( ntdll, #function ) )
			return	bind(NtOpenFile) &&
					bind(NtReadFile) &&
					bind(NtClose) &&
					bind(NtOpenDirectoryObject) &&
					bind(NtQueryDirectoryObject) &&
					bind(RtlAdjustPrivilege);
#			undef bind
		}
		/*	joins \Device\HarddiskVolumeShadowCopyN and \some\file into one nt object path,
			a UNICODE_STRING carries its length so it is never terminator dependent, but we
			keep it zero terminated anyway to stay printable. */
		static bool nt_path( UNICODE_STRING* out, wchar_t* buffer, size_t characters, const wchar_t* device, const wchar_t* path ){
			const size_t devicelength = device ? wcslen( device ) : 0;
			const size_t pathlength = path ? wcslen( path ) : 0;
			const bool separator = ( pathlength != 0 ) && ( path[0] != L'\\' ) && ( device[ devicelength - 1 ] != L'\\' );
			const size_t total = devicelength + separator + pathlength;
			if( ( devicelength == 0 ) || ( total + 1 > characters ) || ( total * sizeof(wchar_t) > 0xfffe ) ){
				return false;	/* Length is an USHORT, a path can never be longer than that */
			}
			wmemcpy( buffer, device, devicelength );
			if( separator ){ buffer[ devicelength ] = L'\\'; }
			wmemcpy( buffer + devicelength + separator, path, pathlength );
			buffer[ total ] = 0;
			*out = (UNICODE_STRING){
				.Length			= (USHORT)( total * sizeof(wchar_t) ),
				.MaximumLength	= (USHORT)( ( total + 1 ) * sizeof(wchar_t) ),
				.Buffer			= buffer
			};
			return true;
		}
		static HANDLE open( const wchar_t* device, const wchar_t* path ){
			wchar_t pathbuffer[ 1024 ];
			UNICODE_STRING objectname;
			if( ! bind_ntdll() || ! nt_path( &objectname, pathbuffer, ARRAYSIZE(pathbuffer), device, path ) ){
				status = STATUS_INVALID_PARAMETER;
				return 0;
			}
			OBJECT_ATTRIBUTES attributes = {
				.Length		= sizeof(attributes),
				.ObjectName	= &objectname,
				.Attributes	= OBJ_CASE_INSENSITIVE
			};
			HANDLE file = 0;
			IO_STATUS_BLOCK iostatus = {};
			status = nt.NtOpenFile(
				&file,
				FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
				&attributes,
				&iostatus,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT
			);
			return status >= 0 ? file : 0;
		}
		static uint32_t read( HANDLE file, void* buffer, uint32_t bytes, uint64_t offset ){
			if( ! bind_ntdll() ){ return 0; }
			IO_STATUS_BLOCK iostatus = {};
			LARGE_INTEGER position = { .QuadPart = (LONGLONG)offset };
			status = nt.NtReadFile( file, 0, 0, 0, &iostatus, buffer, bytes, &position, 0 );
			return status >= 0 ? (uint32_t)iostatus.Information : 0;
		}
		static void close( HANDLE file ){
			if( file && bind_ntdll() ){ nt.NtClose( file ); }
		}
		static uint32_t for_each_device( void(*found)( const wchar_t* device ) ){
			if( ! bind_ntdll() ){ return 0; }
			UNICODE_STRING devicedirectory = {
				.Length			= sizeof(L"\\Device") - sizeof(wchar_t),
				.MaximumLength	= sizeof(L"\\Device"),
				.Buffer			= L"\\Device"
			};
			OBJECT_ATTRIBUTES attributes = {
				.Length		= sizeof(attributes),
				.ObjectName	= &devicedirectory,
				.Attributes	= OBJ_CASE_INSENSITIVE
			};
			HANDLE directory = 0;
			status = nt.NtOpenDirectoryObject( &directory, DIRECTORY_QUERY, &attributes );
			if( status < 0 ){ return 0; }	/* querying \Device needs an elevated token */

			uint32_t count = 0;
			ULONG context = 0;
			/*	one entry per call, the names live in the tail of the same buffer */
			__declspec(align(8)) unsigned char entrybuffer[ 1024 ];
			for(;;){
				ULONG returned = 0;
				status = nt.NtQueryDirectoryObject( directory, entrybuffer, sizeof(entrybuffer), TRUE, count == 0, &context, &returned );
				if( status < 0 ){ break; }
				const OBJECT_DIRECTORY_INFORMATION* const entry = (const OBJECT_DIRECTORY_INFORMATION*)entrybuffer;
				if( entry->Name.Buffer == 0 ){ break; }
				const wchar_t prefix[] = L"HarddiskVolumeShadowCopy";
				const size_t prefixcharacters = ARRAYSIZE(prefix) - 1;
				if( ( entry->Name.Length / sizeof(wchar_t) > prefixcharacters ) &&
					( _wcsnicmp( entry->Name.Buffer, prefix, prefixcharacters ) == 0 ) ){
					wchar_t device[ 256 ] = L"\\Device\\";
					const size_t characters = entry->Name.Length / sizeof(wchar_t);
					if( characters < ARRAYSIZE(device) - ARRAYSIZE(L"\\Device\\") ){
						wmemcpy( device + wcslen(device), entry->Name.Buffer, characters );
						count++;
						if( found ){ found( device ); }
					}
				}
				if( status == STATUS_NO_MORE_ENTRIES ){ break; }
			}
			nt.NtClose( directory );
			return count;
		}
		static bool enable_backup_privilege( void ){
			BOOLEAN wasenabled = FALSE;
			if( ! bind_ntdll() ){ return false; }
			status = nt.RtlAdjustPrivilege( SE_BACKUP_PRIVILEGE, TRUE, FALSE, &wasenabled );
			return status >= 0;
		}
		static long last_status( void ){
			return status;
		}
		typeof(shadowvolume) shadowvolume = {
			.open						= open,
			.read						= read,
			.close						= close,
			.for_each_device			= for_each_device,
			.enable_backup_privilege	= enable_backup_privilege,
			.last_status				= last_status
		};

		/*	clang -DSHADOWVOLUME_MAIN shadowvolume.c -o shadowvolume.exe, run elevated */
#		ifdef SHADOWVOLUME_MAIN
			static void print_device( const wchar_t* device ){
				wprintf( L"found %s\n", device );
			}
			int main( int argc, char* argv[] ){
				(void)argc; (void)argv;
				shadowvolume.enable_backup_privilege();
				if( shadowvolume.for_each_device( print_device ) == 0 ){
					wprintf( L"no shadow copies present (status %08lX), create one first\n", shadowvolume.last_status() );
					return 1;
				}
				/*	the event log is held open by the event log service, the snapshot is a
					point in time copy of the volume, so this read never contends with it. */
				const HANDLE file = shadowvolume.open(
					L"\\Device\\HarddiskVolumeShadowCopy1",
					L"\\Windows\\System32\\winevt\\Logs\\System.evtx"
				);
				if( file == 0 ){
					wprintf( L"open failed with status %08lX\n", shadowvolume.last_status() );
					return 1;
				}
				unsigned char header[ 8 ] = {};
				const uint32_t bytes = shadowvolume.read( file, header, sizeof(header), 0 );
				wprintf( L"read %u bytes: %hs\n", bytes, (char*)header );	/* "ElfFile" */
				shadowvolume.close( file );
				return 0;
			}
#		endif
#	endif
#endif
