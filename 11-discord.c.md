### discord.c

A component that posts a message to a discord channel. It is here because it is the first thing in this repository that has to talk to something outside the machine, and that is where a c project usually stops being dependency free- the reflex is to pull in libcurl, and with it a build step, a dll to ship and a version to keep track of. Windows already has an http client with tls in it, so this component is one file and links nothing.

It is also a second look at the two things the box chapter was about, private state and the transparent union, applied to something that is not a drawing.

### What the caller sees

```c
	typedef union{
		struct discord_arguments{
			const char*	webhook;
			const char*	token;
			const char*	channel;
			const char*	username;
			uint32_t	timeout;
		}discord_arguments;
		struct discord_arguments;
	}discord_arguments;
```

The same named argument construction as `box.new`- a struct of everything the component might need, filled in by designated initializer at the call site, so the caller writes the names of the two fields that matter and the rest is zero:

```c
	auto channel = discord.new(( struct discord_arguments ){ .webhook = getenv("COOPE_WEBHOOK") });
```

There are two ways into discord and both are here. A webhook is a url that carries its own credential in the path- anyone holding it can post to that one channel and to nothing else, which makes it the right thing for a program that only wants to report. A bot token authorizes posting to every channel the bot was added to, and needs the channel id alongside it. Neither of them belongs in the source, so neither is in it: they come in as arguments, and `getenv` above is the point.

```c
	typedef union{
		struct discord_interface{
			union{
				struct discord_vtable{
					struct discord_response (*send)( struct discord_interface* this, struct string content );
					struct discord_response (*post)( struct discord_interface* this, struct discord_embed embed );
					void (*close)( struct discord_interface* this );
				}discord_vtable;
				struct discord_vtable;
			};
		}discord_interface;
		struct discord_interface;
	}discord_interface;
```

What `new` hands back. Three function pointers and nothing else- there is no session handle in there, no host, no authorization header, and no way for the caller to reach one. Same shape as `box_interface`, and for the same reason.

### The private half

```c
		typedef struct channel{
			discord_interface;
			HINTERNET		session;
			const char*		username;
			wchar_t			host[ 128 ];
			wchar_t			path[ 512 ];
			wchar_t			headers[ 256 ];
			INTERNET_PORT	port;
			bool			secure;
		}channel;
		union transparent converter{ struct channel* channel; struct discord_interface* interface; };
```

`struct channel` is declared inside the `__INCLUDE_LEVEL__ == 0` block, so it does not exist for anyone including this file- the type is not incomplete to them, it is not there at all. The interface is its first member, so the two pointers address the same byte, and the transparent union makes that fact usable without a cast:

```c
		static struct discord_response send( union converter this, struct string content ){
			char body[ BODYSIZE ];
			const uint32_t length = this.channel
				? message_body( body, sizeof(body), this.channel->username, content )
				: 0;
			return post_with_retry( this.channel, body, length );
		}
```

`send` is assigned to a field whose type takes a `struct discord_interface*`, and it is written taking the union, which has that pointer as a member- so the assignment is accepted, and inside the function `this.channel` is a `struct channel*` with the private members on it. The box chapter did this with a forward declaration and a definition that differed in the first parameter; here the union is the parameter of the definition too, which reads more plainly and does the same thing. No cast appears anywhere in the file, and no `void*`.

The one rule that comes with it: the interface has to stay the first member of the private struct. That is the whole contract, and it is one line apart from the thing it constrains.

### Binding winhttp instead of linking it

```c
			const HMODULE module = LoadLibraryW( L"winhttp.dll" );
			if( module == 0 ){
				lasterror = GetLastError();
				return false;
			}
#			define bind(function) ( winhttp.function = (typeof(winhttp.function))GetProcAddress( module, #function ) )
			return	bind(WinHttpOpen) &&
					bind(WinHttpSetTimeouts) &&
					...
```

The same `bind` macro as the nt components, with `typeof` on the struct member so the cast is written once and can never drift from the declaration. `GetProcAddress` returns a `FARPROC` and the field says what it should be, so there is exactly one place holding the signature.

The difference from ntdll is `LoadLibraryW`. Ntdll is mapped into every process before the first instruction of the program runs, so `GetModuleHandleW` is enough for it. Winhttp is not, and has to be brought in. It is never unloaded- the process wants it for as long as it wants to talk, and unloading a library to reload it on the next message would cost more than keeping it.

Nothing in the project file changes for this, no `winhttp.lib`, and a machine without the dll gets a `new` that returns 0 rather than a program that will not start.

The declarations winhttp needs are in the file too, behind a check for winhttp.h's own include guard, so including the real header instead does not collide with them.

### Parsing the url

`WinHttpCrackUrl` exists and does this, and I do it by hand anyway:

```c
			if( strncmp( rest, "https://", 8 ) == 0 ){
				rest += 8;
				into->secure = true;
				into->port = INTERNET_DEFAULT_HTTPS_PORT;
			}else if( strncmp( rest, "http://", 7 ) == 0 ){
				...
			}else{
				return false;	/*	a url without a scheme is a guess, and this one carries a secret */
			}
			const size_t hostlength = strcspn( rest, ":/" );
```

`WinHttpCrackUrl` wants a `URL_COMPONENTS` filled in a particular way, hands back pointers into the string it was given rather than copies, and needs a second pass to make them terminated- for splitting three fields off a url that is always the same shape it is more api than the problem has.

The scheme is required rather than assumed. A url with no scheme would have to default to something, and defaulting to http would put a bot token on the wire in cleartext because someone left eight characters off a string in a config file. That is not a default worth having, so it is an error instead.

### Escaping, and the size of the buffer

```c
					case '"':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = '"';	break;
					...
					default:
						if( character < 0x20 ){
							out = out + (uint16_t)snprintf( &escaped.array[ out ], 7, "\\u%04x", character );
						}else{
							escaped.array[ out++ ] = (char)character;
						}
```

Seven characters have to be spelled differently inside a json string, everything else below a space has to go out as `\u00xx`, and that is the whole of it. Bytes above 0x7f are passed through untouched- utf-8 continuation bytes are already legal json, and the console is on code page 65001, so what the caller is holding is utf-8 before this ever sees it.

The loop stops at `ARRSIZE - 7` rather than at `ARRSIZE`, because the longest thing one input character can turn into is the six characters of `\u001f` plus a terminator. Checking the room for the worst case before the switch means the switch itself never has to.

The json body then gets its own buffer rather than a `struct string`:

```c
#		define BODYSIZE 4096
```

A `struct string` is `ARRSIZE` bytes, and escaping can grow what it is given. A message of nothing but quotes doubles, and one of control characters is six times its own length. Sizing the body buffer for the text rather than for the escaping is the kind of arithmetic that holds for every message anyone actually sends and then does not.

```c
		static uint32_t written( int result, uint32_t size ){
			return ( ( result > 0 ) && ( (uint32_t)result < size ) ) ? (uint32_t)result : 0;
		}
```

`snprintf` reports the length it wanted, not the length it wrote, so a body that did not fit looks like a long one unless someone compares. `written` does the comparing in one place and turns the overflow into a 0, and `request` refuses to send a body of length 0- a truncated json document is never put on the wire.

### The response

```c
	typedef union{
		struct discord_response{
			uint16_t		status;
			uint32_t		retry_after;
			struct string	body;
		}discord_response;
		struct discord_response;
	}discord_response;
```

Returned by value, so there is nothing to free and nothing to check for null. `status` is the http status and `0` means the request never got as far as discord, in which case `discord.last_error()` holds the win32 error- one field distinguishing "discord said no" from "the network said no" is worth more than a bool.

A webhook post that worked answers 204 with an empty body, so `status` is usually the only part worth reading.

```c
			if( ( response.status == 429 ) && ( response.retry_after != 0 ) && ( response.retry_after <= 5000 ) ){
				Sleep( response.retry_after );
				response = request( this, body, length );
			}
```

Being rate limited is not an error, it is discord telling us how long to wait, and the second attempt is the one that lands. It waits once and only for a wait short enough to block a tick on- anything longer is handed back in `retry_after` for the caller to decide about, because a component has no business sleeping a thread for a minute on its own.

### The session

`new` opens the winhttp session and keeps it on the channel, and each message opens a connection and a request handle and closes them again. The session is what holds the resolved name and the tls session, so the second message does not repeat the handshake, and the per message handles are what makes a failed message a failed message rather than a broken channel.

Opening the session in `new` also moves every failure that is not the network to construction. A caller holding an interface knows the url parsed, winhttp is present and the session is up, which is the same argument for constructors that the introduction makes- if the object exists, it is usable.

`close` closes the session and frees the channel. There is no destructor running for us, so it is one call, and it is on the interface where the caller can see it.

### Testing it without a network

```c
		test(discord){
			assert( strcmp( discord.escape( str("plain text") ).array, "plain text" ) == 0 );
			assert( strcmp( discord.escape( str("say \"hi\"") ).array, "say \\\"hi\\\"" ) == 0 );
			assert( strcmp( discord.escape( str("one\ntwo") ).array, "one\\ntwo" ) == 0 );
			assert( strcmp( discord.escape( str("c:\\temp") ).array, "c:\\\\temp" ) == 0 );
			assert( strcmp( discord.escape( str("\x01") ).array, "\\u0001" ) == 0 );
		}
```

`escape` is on the public interface for this reason alone. It is the part of the component that is pure text in and text out, it is the part that a mistake in produces a body discord rejects with a 400 and no explanation, and it is the only part that can be checked without a token and a network. The rest of the file is winhttp calls, and a test of those would be a test of winhttp.

### Using it

```c
	static struct discord_interface* reporting;
	static HANDLE reporttick( struct timer* ){
		static __thread int reported = 0;
		reporting->send( reporting, str("boxes drawn: ", str(++reported)) );
		return 0;
	}
	int main( int argc, char* argv[] ){
		reporting = discord.new(( struct discord_arguments ){
			.webhook	= getenv("COOPE_WEBHOOK"),
			.username	= "coope",
			.timeout	= 5000
		});
		if( reporting ){ timers.new( reporttick, 60000 ); }
		timers.new( drawtick, 2 );
		console.run();
	}
```

A channel is not shared between threads, so a timer that reports gets its own- the session handle is winhttp's to be careful with, but the request handles around it are not, and one channel per thread is cheaper to guarantee than a lock is to hold. `timers.new` gives each timer its own thread already, so this is one line either way.
