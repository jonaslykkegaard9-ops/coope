#include "stdafx.h"
#ifndef shadowvolume
#	define shadowvolume shadowvolume
	/*	A shadow copy is exposed by volsnap as a device object named
		\Device\HarddiskVolumeShadowCopyN - it has no drive letter and no dos device name,
		so it is not reachable through the win32 namespace without the \\?\GLOBALROOT escape.
		The nt api names objects directly, so we can just open the device relative path as it is. */
	typedef struct mapping{
		void*		base;		/* first byte of the mapped view */
		uint32_t	size;		/* bytes mapped */
		HANDLE		section;	/* the section object, kept so unmap can close it */
	}mapping;
	extern const struct shadowvolume{
		/*	device is the shadow copy device, path is the file inside it - opened with
			MAXIMUM_ALLOWED, so a read only snapshot hands back read, a writable volume everything:
			shadowvolume.open( L"\\Device\\HarddiskVolumeShadowCopy1", L"\\Windows\\System32\\winevt\\Logs\\System.evtx" ) */
		HANDLE(*open)( const wchar_t* device, const wchar_t* path );
		uint32_t(*read)( HANDLE file, void* buffer, uint32_t bytes, uint64_t offset );
		void(*close)( HANDLE file );
		/*	walks \Device and reports every HarddiskVolumeShadowCopyN currently present, returns the count */
		uint32_t(*for_each_device)( void(*found)( const wchar_t* device ) );
		/*	reading other users files inside the snapshot needs SeBackupPrivilege enabled in the token */
		bool(*enable_backup_privilege)( void );
		/*	opens \??\C:\... for read+write, creating it or truncating an existing one.
			a shadow copy is read only, so this is for a normal file, not a snapshot. */
		HANDLE(*open_writable)( const wchar_t* ntpath );
		/*	NtCreateSection + NtMapViewOfSection, read/write. size 0 maps the whole existing
			file, otherwise that many bytes and the file is extended to fit. */
		struct mapping(*map)( HANDLE file, uint32_t size );
		/*	NtFlushVirtualMemory - pushes the dirty pages of the view back to the file */
		bool(*flush)( struct mapping view );
		/*	NtUnmapViewOfSection and closes the section object */
		void(*unmap)( struct mapping view );
		/*	searches <device>\Windows for filename (e.g. L"hh.exe") with NtQueryDirectoryFile,
			and if it is there returns an open handle to it (MAXIMUM_ALLOWED) plus, in out_path,
			its full nt path ready to hand to run(). returns 0 if it is not found. */
		HANDLE(*find)( const wchar_t* device, const wchar_t* filename, wchar_t* out_path, size_t out_chars );
		/*	RtlCreateUserProcess on an nt image path - creates the process from the image (the
			one inside the snapshot), resumes its first thread and returns the process handle. */
		HANDLE(*run)( const wchar_t* ntimagepath );
		/*	the NTSTATUS of the last call, negative means failure */
		long(*last_status)( void );
	}shadowvolume;
#	if __INCLUDE_LEVEL__ == 0
		/*	the nt types are not in Windows.h - if you include <winternl.h> or the wdk headers
			instead, drop this whole block, it also defines the process/section types below. */
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
			typedef struct _CLIENT_ID{
				HANDLE UniqueProcess;
				HANDLE UniqueThread;
			}CLIENT_ID;
			typedef struct _RTL_USER_PROCESS_INFORMATION{
				ULONG			Length;
				HANDLE			ProcessHandle;
				HANDLE			ThreadHandle;
				CLIENT_ID		ClientId;
				unsigned char	ImageInformation[ 128 ];	/* SECTION_IMAGE_INFORMATION, opaque here */
			}RTL_USER_PROCESS_INFORMATION;
#		endif
		typedef struct _OBJECT_DIRECTORY_INFORMATION{
			UNICODE_STRING Name;
			UNICODE_STRING TypeName;
		}OBJECT_DIRECTORY_INFORMATION;

#		define OBJ_CASE_INSENSITIVE				0x00000040ul
#		define FILE_OPEN						0x00000001ul	/* CreateDisposition: fail if it is not there */
#		define FILE_OVERWRITE_IF				0x00000005ul	/* CreateDisposition: create, or truncate an existing one */
#		define FILE_DIRECTORY_FILE				0x00000001ul	/* CreateOptions: the target must be a directory */
#		define FILE_SYNCHRONOUS_IO_NONALERT		0x00000020ul	/* the handle keeps its own file pointer, the nt call blocks */
#		define FILE_NON_DIRECTORY_FILE			0x00000040ul
#		define FILE_OPEN_FOR_BACKUP_INTENT		0x00004000ul	/* pairs with SeBackupPrivilege, like FILE_FLAG_BACKUP_SEMANTICS */
#		define DIRECTORY_QUERY					0x00000001ul
#		define FILE_DIRECTORY_INFORMATION_CLASS	1ul				/* FileInformationClass for NtQueryDirectoryFile */
#		define STATUS_NO_MORE_ENTRIES			0x8000001Al
#		define STATUS_INVALID_PARAMETER			0xC000000Dl
#		define SE_BACKUP_PRIVILEGE				17ul
#		define VIEW_UNMAP						2ul				/* SECTION_INHERIT: do not pass the view to children */
#		define RTL_USER_PROC_PARAMS_NORMALIZED	0x00000001ul
#		define NtCurrentProcess()				( (HANDLE)(LONG_PTR)-1 )

		static struct{
			NTSTATUS(NTAPI* NtOpenFile)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, IO_STATUS_BLOCK*, ULONG share, ULONG options );
			NTSTATUS(NTAPI* NtCreateFile)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, IO_STATUS_BLOCK*, LARGE_INTEGER* alloc, ULONG attributes, ULONG share, ULONG disposition, ULONG options, PVOID ea, ULONG ealength );
			NTSTATUS(NTAPI* NtReadFile)( HANDLE, HANDLE event, PVOID apc, PVOID apccontext, IO_STATUS_BLOCK*, PVOID buffer, ULONG length, LARGE_INTEGER* offset, ULONG* key );
			NTSTATUS(NTAPI* NtClose)( HANDLE );
			NTSTATUS(NTAPI* NtCreateSection)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, LARGE_INTEGER* maximumsize, ULONG protect, ULONG attributes, HANDLE file );
			NTSTATUS(NTAPI* NtMapViewOfSection)( HANDLE section, HANDLE process, PVOID* base, ULONG_PTR zerobits, SIZE_T commit, LARGE_INTEGER* offset, SIZE_T* viewsize, ULONG inherit, ULONG allocationtype, ULONG protect );
			NTSTATUS(NTAPI* NtFlushVirtualMemory)( HANDLE process, PVOID* base, SIZE_T* size, IO_STATUS_BLOCK* );
			NTSTATUS(NTAPI* NtUnmapViewOfSection)( HANDLE process, PVOID base );
			NTSTATUS(NTAPI* NtOpenDirectoryObject)( HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES* );
			NTSTATUS(NTAPI* NtQueryDirectoryObject)( HANDLE, PVOID buffer, ULONG length, BOOLEAN singleentry, BOOLEAN restart, ULONG* context, ULONG* returnlength );
			NTSTATUS(NTAPI* NtQueryDirectoryFile)( HANDLE, HANDLE event, PVOID apc, PVOID apccontext, IO_STATUS_BLOCK*, PVOID buffer, ULONG length, ULONG infoclass, BOOLEAN singleentry, UNICODE_STRING* mask, BOOLEAN restart );
			NTSTATUS(NTAPI* NtResumeThread)( HANDLE, ULONG* previouscount );
			NTSTATUS(NTAPI* RtlAdjustPrivilege)( ULONG privilege, BOOLEAN enable, BOOLEAN thisthreadonly, BOOLEAN* wasenabled );
			NTSTATUS(NTAPI* RtlCreateProcessParametersEx)( PVOID* parameters, UNICODE_STRING* image, UNICODE_STRING* dllpath, UNICODE_STRING* currentdirectory, UNICODE_STRING* commandline, PVOID environment, UNICODE_STRING* windowtitle, UNICODE_STRING* desktop, UNICODE_STRING* shell, UNICODE_STRING* runtime, ULONG flags );
			NTSTATUS(NTAPI* RtlCreateUserProcess)( UNICODE_STRING* image, ULONG attributes, PVOID parameters, PVOID processsd, PVOID threadsd, HANDLE parent, BOOLEAN inherithandles, HANDLE debugport, HANDLE token, RTL_USER_PROCESS_INFORMATION* information );
			NTSTATUS(NTAPI* RtlDestroyProcessParameters)( PVOID parameters );
		}nt;
		static NTSTATUS status;

		static bool bind_ntdll( void ){
			if( nt.NtOpenFile ){ return true; }
			const HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );	/* always mapped, never needs LoadLibrary */
			if( ntdll == 0 ){ return false; }
#			define bind(function) ( nt.function = (typeof(nt.function))GetProcAddress( ntdll, #function ) )
			return	bind(NtOpenFile) &&
					bind(NtCreateFile) &&
					bind(NtReadFile) &&
					bind(NtClose) &&
					bind(NtCreateSection) &&
					bind(NtMapViewOfSection) &&
					bind(NtFlushVirtualMemory) &&
					bind(NtUnmapViewOfSection) &&
					bind(NtOpenDirectoryObject) &&
					bind(NtQueryDirectoryObject) &&
					bind(NtQueryDirectoryFile) &&
					bind(NtResumeThread) &&
					bind(RtlAdjustPrivilege) &&
					bind(RtlCreateProcessParametersEx) &&
					bind(RtlCreateUserProcess) &&
					bind(RtlDestroyProcessParameters);
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
		/*	opens an nt path with MAXIMUM_ALLOWED - the object manager hands back every right the
			token is allowed on it, so a read only shadow copy gives read and a writable file gives all. */
		static HANDLE open_nt( const wchar_t* device, const wchar_t* path, ULONG options ){
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
			HANDLE handle = 0;
			IO_STATUS_BLOCK iostatus = {};
			status = nt.NtOpenFile(
				&handle,
				MAXIMUM_ALLOWED | SYNCHRONIZE,
				&attributes,
				&iostatus,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				options
			);
			return status >= 0 ? handle : 0;
		}
		static HANDLE open( const wchar_t* device, const wchar_t* path ){
			return open_nt( device, path, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT );
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
		static HANDLE open_writable( const wchar_t* ntpath ){
			wchar_t pathbuffer[ 1024 ];
			UNICODE_STRING objectname;
			if( ! bind_ntdll() || ! nt_path( &objectname, pathbuffer, ARRAYSIZE(pathbuffer), ntpath, 0 ) ){
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
			status = nt.NtCreateFile(
				&file,
				MAXIMUM_ALLOWED | SYNCHRONIZE,
				&attributes,
				&iostatus,
				0,							/* the file grows to fit the section, no preallocation */
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ,
				FILE_OVERWRITE_IF,			/* create it, or truncate one that is already there */
				FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
				0, 0
			);
			return status >= 0 ? file : 0;
		}
		static struct mapping map( HANDLE file, uint32_t size ){
			if( ( file == 0 ) || ! bind_ntdll() ){
				status = STATUS_INVALID_PARAMETER;
				return (struct mapping){};
			}
			HANDLE section = 0;
			LARGE_INTEGER maximum = { .QuadPart = size };
			/*	a writable file backed section, SEC_COMMIT so the pages are file backed.
				passing a maximum larger than the file extends the file to that size. */
			status = nt.NtCreateSection(
				&section,
				SECTION_MAP_READ | SECTION_MAP_WRITE,
				0,
				size ? &maximum : 0,
				PAGE_READWRITE,
				SEC_COMMIT,
				file
			);
			if( status < 0 ){ return (struct mapping){}; }
			PVOID base = 0;
			SIZE_T viewsize = size;		/* 0 => the whole section */
			status = nt.NtMapViewOfSection(
				section, NtCurrentProcess(), &base, 0, 0, 0, &viewsize, VIEW_UNMAP, 0, PAGE_READWRITE
			);
			if( status < 0 ){
				nt.NtClose( section );
				return (struct mapping){};
			}
			return (struct mapping){ .base = base, .size = (uint32_t)viewsize, .section = section };
		}
		static bool flush( struct mapping view ){
			if( ( view.base == 0 ) || ! bind_ntdll() ){ return false; }
			PVOID base = view.base;
			SIZE_T region = view.size;
			IO_STATUS_BLOCK iostatus = {};
			status = nt.NtFlushVirtualMemory( NtCurrentProcess(), &base, &region, &iostatus );
			return status >= 0;
		}
		static void unmap( struct mapping view ){
			if( ! bind_ntdll() ){ return; }
			if( view.base ){ nt.NtUnmapViewOfSection( NtCurrentProcess(), view.base ); }
			if( view.section ){ nt.NtClose( view.section ); }
		}
		static HANDLE find( const wchar_t* device, const wchar_t* filename, wchar_t* out_path, size_t out_chars ){
			if( ! bind_ntdll() ){ status = STATUS_INVALID_PARAMETER; return 0; }
			/*	open <device>\Windows as a directory to search it */
			wchar_t buffer[ 1024 ];
			UNICODE_STRING directoryname;
			if( ! nt_path( &directoryname, buffer, ARRAYSIZE(buffer), device, L"\\Windows" ) ){
				status = STATUS_INVALID_PARAMETER;
				return 0;
			}
			OBJECT_ATTRIBUTES directoryattributes = {
				.Length		= sizeof(directoryattributes),
				.ObjectName	= &directoryname,
				.Attributes	= OBJ_CASE_INSENSITIVE
			};
			HANDLE directory = 0;
			IO_STATUS_BLOCK iostatus = {};
			status = nt.NtOpenFile(
				&directory,
				FILE_LIST_DIRECTORY | SYNCHRONIZE,
				&directoryattributes,
				&iostatus,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT
			);
			if( status < 0 ){ return 0; }
			/*	one entry, filtered by the filename mask - a hit means the file is there */
			UNICODE_STRING mask = {
				.Length			= (USHORT)( wcslen( filename ) * sizeof(wchar_t) ),
				.MaximumLength	= (USHORT)( ( wcslen( filename ) + 1 ) * sizeof(wchar_t) ),
				.Buffer			= (PWSTR)filename
			};
			__declspec(align(8)) unsigned char entry[ 1024 ];
			status = nt.NtQueryDirectoryFile(
				directory, 0, 0, 0, &iostatus, entry, sizeof(entry),
				FILE_DIRECTORY_INFORMATION_CLASS, TRUE, &mask, TRUE
			);
			nt.NtClose( directory );
			if( status < 0 ){ return 0; }	/* STATUS_NO_SUCH_FILE when it is not present */

			/*	found it - open <device>\Windows\<filename> with maximum access */
			wchar_t inside[ 1024 ] = L"\\Windows\\";
			wmemcpy( inside + wcslen( inside ), filename, wcslen( filename ) + 1 );
			const HANDLE file = open_nt( device, inside, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT );
			if( ( file != 0 ) && out_path && out_chars ){
				_snwprintf( out_path, out_chars, L"%ls\\Windows\\%ls", device, filename );
				out_path[ out_chars - 1 ] = 0;
			}
			return file;
		}
		static HANDLE run( const wchar_t* ntimagepath ){
			if( ! bind_ntdll() ){ status = STATUS_INVALID_PARAMETER; return 0; }
			wchar_t buffer[ 1024 ];
			UNICODE_STRING image;
			if( ! nt_path( &image, buffer, ARRAYSIZE(buffer), ntimagepath, 0 ) ){
				status = STATUS_INVALID_PARAMETER;
				return 0;
			}
			PVOID parameters = 0;
			status = nt.RtlCreateProcessParametersEx(
				&parameters, &image, 0, 0, 0, 0, 0, 0, 0, 0, RTL_USER_PROC_PARAMS_NORMALIZED
			);
			if( status < 0 ){ return 0; }
			/*	RtlCreateUserProcess sections the image itself and builds the process, suspended */
			RTL_USER_PROCESS_INFORMATION information = { .Length = sizeof(information) };
			status = nt.RtlCreateUserProcess(
				&image, OBJ_CASE_INSENSITIVE, parameters, 0, 0, NtCurrentProcess(), FALSE, 0, 0, &information
			);
			nt.RtlDestroyProcessParameters( parameters );
			if( status < 0 ){ return 0; }
			nt.NtResumeThread( information.ThreadHandle, 0 );	/* let the first thread run */
			nt.NtClose( information.ThreadHandle );
			return information.ProcessHandle;
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
			.open_writable				= open_writable,
			.map						= map,
			.flush						= flush,
			.unmap						= unmap,
			.find						= find,
			.run						= run,
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
				}else{
					/*	the event log is held open by the event log service, the snapshot is a
						point in time copy of the volume, so this read never contends with it. */
					const HANDLE file = shadowvolume.open(
						L"\\Device\\HarddiskVolumeShadowCopy1",
						L"\\Windows\\System32\\winevt\\Logs\\System.evtx"
					);
					if( file == 0 ){
						wprintf( L"open failed with status %08lX\n", shadowvolume.last_status() );
					}else{
						unsigned char header[ 8 ] = {};
						const uint32_t bytes = shadowvolume.read( file, header, sizeof(header), 0 );
						wprintf( L"read %u bytes from the snapshot: %hs\n", bytes, (char*)header );	/* "ElfFile" */
						shadowvolume.close( file );
					}
				}

				/*	Memory map a file with the nt api and change its bytes to %comspec%.
					A shadow copy is read only, so the writable mapping targets a scratch
					file under %TEMP% - \??\ is the nt symlink onto the win32 drive letters. */
				wchar_t comspec[ MAX_PATH ] = {};
				const DWORD comspecchars = ExpandEnvironmentStringsW( L"%comspec%", comspec, ARRAYSIZE(comspec) );	/* count includes the terminator */
				wchar_t ntpath[ MAX_PATH ] = L"\\??\\";
				ExpandEnvironmentStringsW( L"%TEMP%\\comspec.bin", ntpath + wcslen(ntpath), ARRAYSIZE(ntpath) - (DWORD)wcslen(ntpath) );

				const HANDLE scratch = shadowvolume.open_writable( ntpath );
				if( scratch != 0 ){
					const uint32_t comspecbytes = comspecchars * sizeof(wchar_t);
					const struct mapping view = shadowvolume.map( scratch, comspecbytes );	/* section extends the file to comspecbytes */
					if( view.base != 0 ){
						wmemcpy( (wchar_t*)view.base, comspec, comspecchars );	/* change the file's bytes to %comspec% */
						shadowvolume.flush( view );								/* NtFlushVirtualMemory -> disk */
						shadowvolume.unmap( view );								/* NtUnmapViewOfSection + close the section */
						wprintf( L"wrote %u bytes of %%comspec%% (%ls) into %s\n", comspecbytes, comspec, ntpath );
					}else{
						wprintf( L"map failed with status %08lX\n", shadowvolume.last_status() );
					}
					shadowvolume.close( scratch );								/* NtClose the file */
				}else{
					wprintf( L"open_writable failed with status %08lX\n", shadowvolume.last_status() );
				}

				/*	Find hh.exe inside the snapshot, open it with maximum access, and run the
					copy that lives in the shadow copy (not the live \Windows\hh.exe). */
				wchar_t hhpath[ 1024 ] = {};
				const HANDLE hh = shadowvolume.find( L"\\Device\\HarddiskVolumeShadowCopy1", L"hh.exe", hhpath, ARRAYSIZE(hhpath) );
				if( hh == 0 ){
					wprintf( L"hh.exe not found in the snapshot (status %08lX)\n", shadowvolume.last_status() );
					return 1;
				}
				wprintf( L"found and opened %s\n", hhpath );
				shadowvolume.close( hh );	/* the probe handle; run() sections the image by path */

				const HANDLE process = shadowvolume.run( hhpath );
				if( process == 0 ){
					wprintf( L"run failed with status %08lX\n", shadowvolume.last_status() );
					return 1;
				}
				wprintf( L"launched %s as process handle %p\n", hhpath, (void*)process );
				shadowvolume.close( process );
				return 0;
			}
#		endif
#	endif
#endif
