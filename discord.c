#include "stdafx.h"
#ifndef discord
#	define discord discord
#	include "string.c"
	/*	Discord accepts a message over plain https, so a component that can post one needs no
		library and no dependency- it needs a socket with tls on it, and winhttp is the one
		windows already ships and already knows the proxy settings of the machine.

		There are two ways in, and this component covers both:
		- a webhook, a url that carries its own credential in the path. Anyone holding the url
		  can post to that one channel, and to nothing else.
		- a bot token, which authorizes a POST to /api/v10/channels/<id>/messages for every
		  channel the bot has been added to.
		Neither belongs in the source- both are secrets, so they come in as arguments and the
		caller is expected to read them from the environment or from a file that is not committed. */
	typedef union{
		struct discord_arguments{
			const char*	webhook;	/*	full https url, https://discord.com/api/webhooks/<id>/<token> */
			const char*	token;		/*	bot token, used instead of a webhook */
			const char*	channel;	/*	channel id, only meaningful together with a bot token */
			const char*	username;	/*	overrides the name a webhook posts under, optional */
			uint32_t	timeout;	/*	milliseconds per network operation, 0 keeps the winhttp default */
		}discord_arguments;
		struct discord_arguments;
	}discord_arguments;
	/*	0xrrggbb, the same layout discord uses for the colored bar down the left of an embed */
	typedef union{
		struct discord_embed{
			const char*	title;
			const char*	description;
			uint32_t	color;
		}discord_embed;
		struct discord_embed;
	}discord_embed;
	/*	what came back. status is the http status, and 0 means the request never reached discord-
		last_error() then holds the win32 error. A webhook post that worked answers 204 with an
		empty body, the bot api answers 200 and the message it created. */
	typedef union{
		struct discord_response{
			uint16_t		status;
			uint32_t		retry_after;	/*	milliseconds discord asked us to wait, only set on 429 */
			struct string	body;			/*	as much of the answer as fits in a struct string */
		}discord_response;
		struct discord_response;
	}discord_response;
	/*	the public half of a channel- three function pointers and nothing else, so the caller can
		neither see nor touch the session handle, the parsed url or the authorization header. */
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
	extern const struct discord{
		/*	opens one channel and keeps a winhttp session alive for it:
			discord.new(( struct discord_arguments ){ .webhook = getenv("COOPE_WEBHOOK") })
			returns 0 when the arguments name no destination, the url does not parse, or winhttp
			is not there. A channel is not shared between threads- give each timer its own. */
		struct discord_interface* (*new)( struct discord_arguments arguments );
		/*	the json escaping both body builders use, exposed because it is the one part of this
			component that can be tested without a network. */
		struct string (*escape)( struct string text );
		/*	GetLastError from the last winhttp call that failed */
		unsigned long (*last_error)( void );
	}discord;
#	if __INCLUDE_LEVEL__ == 0
		/*	winhttp.h is not pulled in by Windows.h and neither is its import library, so the few
			declarations this needs are here and the entry points are bound at runtime. Include
			<winhttp.h> and link winhttp.lib instead and this whole block can go. */
#		if !defined(_WINHTTPX_) && !defined(_WININET_)
			typedef void*		HINTERNET;
			typedef uint16_t	INTERNET_PORT;
#			define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY	0ul
#			define WINHTTP_FLAG_SECURE					0x00800000ul
#			define WINHTTP_QUERY_STATUS_CODE			19ul
#			define WINHTTP_QUERY_CUSTOM					65535ul
#			define WINHTTP_QUERY_FLAG_NUMBER			0x20000000ul
#			define INTERNET_DEFAULT_HTTP_PORT			80
#			define INTERNET_DEFAULT_HTTPS_PORT			443
#		endif
		/*	a message is 2000 characters at most and every one of them can come out of the
			escaping as six, so the json buffer is sized for the escaping and not for the text. */
#		define BODYSIZE 4096

		static struct{
			HINTERNET(WINAPI* WinHttpOpen)( const wchar_t* agent, ULONG accesstype, const wchar_t* proxy, const wchar_t* bypass, ULONG flags );
			BOOL(WINAPI* WinHttpSetTimeouts)( HINTERNET, int resolve, int connect, int send, int receive );
			HINTERNET(WINAPI* WinHttpConnect)( HINTERNET session, const wchar_t* servername, INTERNET_PORT port, ULONG reserved );
			HINTERNET(WINAPI* WinHttpOpenRequest)( HINTERNET connection, const wchar_t* verb, const wchar_t* object, const wchar_t* version, const wchar_t* referrer, const wchar_t** accepttypes, ULONG flags );
			BOOL(WINAPI* WinHttpSendRequest)( HINTERNET request, const wchar_t* headers, ULONG headerslength, void* optional, ULONG optionallength, ULONG totallength, ULONG_PTR context );
			BOOL(WINAPI* WinHttpReceiveResponse)( HINTERNET request, void* reserved );
			BOOL(WINAPI* WinHttpQueryHeaders)( HINTERNET request, ULONG infolevel, const wchar_t* name, void* buffer, ULONG* bufferlength, ULONG* index );
			BOOL(WINAPI* WinHttpReadData)( HINTERNET request, void* buffer, ULONG toread, ULONG* read );
			BOOL(WINAPI* WinHttpCloseHandle)( HINTERNET handle );
		}winhttp;
		static unsigned long lasterror;

		static bool bind_winhttp( void ){
			if( winhttp.WinHttpOpen ){ return true; }
			/*	unlike ntdll this one is not already mapped, so it has to be brought in- and it is
				kept for the life of the process, there is nothing to unload it for. */
			const HMODULE module = LoadLibraryW( L"winhttp.dll" );
			if( module == 0 ){
				lasterror = GetLastError();
				return false;
			}
#			define bind(function) ( winhttp.function = (typeof(winhttp.function))GetProcAddress( module, #function ) )
			return	bind(WinHttpOpen) &&
					bind(WinHttpSetTimeouts) &&
					bind(WinHttpConnect) &&
					bind(WinHttpOpenRequest) &&
					bind(WinHttpSendRequest) &&
					bind(WinHttpReceiveResponse) &&
					bind(WinHttpQueryHeaders) &&
					bind(WinHttpReadData) &&
					bind(WinHttpCloseHandle);
#			undef bind
		}
		/*	the private half. The interface is the first member, so a struct channel* and a
			struct discord_interface* address the same byte and the transparent union below can
			carry either one without a cast. */
		typedef struct channel{
			discord_interface;
			HINTERNET		session;
			const char*		username;
			wchar_t			host[ 128 ];
			wchar_t			path[ 512 ];
			wchar_t			headers[ 256 ];	/*	the content type, and the authorization when there is a token */
			INTERNET_PORT	port;
			bool			secure;
		}channel;
		union transparent converter{ struct channel* channel; struct discord_interface* interface; };

		/*	winhttp wants the host and the path apart, and as wide strings. The url is ascii- a
			hostname that is not would have to be punycoded before it got here. */
		static bool split_url( const char* url, struct channel* into ){
			if( url == 0 ){ return false; }
			const char* rest = url;
			if( strncmp( rest, "https://", 8 ) == 0 ){
				rest += 8;
				into->secure = true;
				into->port = INTERNET_DEFAULT_HTTPS_PORT;
			}else if( strncmp( rest, "http://", 7 ) == 0 ){
				rest += 7;
				into->secure = false;
				into->port = INTERNET_DEFAULT_HTTP_PORT;
			}else{
				return false;	/*	a url without a scheme is a guess, and this one carries a secret */
			}
			const size_t hostlength = strcspn( rest, ":/" );
			if( ( hostlength == 0 ) || ( hostlength + 1 > ARRAYSIZE(into->host) ) ){ return false; }
			for( size_t i = 0; i < hostlength; i++ ){ into->host[ i ] = (wchar_t)rest[ i ]; }
			into->host[ hostlength ] = 0;
			rest += hostlength;
			if( *rest == ':' ){
				unsigned long parsed = 0;
				for( rest++; ( *rest >= '0' ) && ( *rest <= '9' ); rest++ ){
					parsed = ( parsed * 10 ) + (unsigned long)( *rest - '0' );
				}
				if( ( parsed == 0 ) || ( parsed > 0xffff ) ){ return false; }
				into->port = (INTERNET_PORT)parsed;
			}
			const char* const path = ( *rest == 0 ) ? "/" : rest;
			const size_t pathlength = strlen( path );
			if( pathlength + 1 > ARRAYSIZE(into->path) ){ return false; }
			for( size_t i = 0; i <= pathlength; i++ ){ into->path[ i ] = (wchar_t)path[ i ]; }
			return true;
		}
		/*	json has seven characters that have to be spelled differently inside a string, and
			everything below a space that is not one of them has to go out as \u00xx. Bytes above
			0x7f are left alone- utf-8 continuation bytes are already legal json, and the console
			is on code page 65001, so what the caller holds is utf-8 to begin with. */
		static struct string escape( struct string text ){
			struct string escaped = {0};
			uint16_t out = 0;
			for( uint16_t in = 0; ( text.array[ in ] != 0 ) && ( out < ( ARRSIZE - 7 ) ); in++ ){
				const unsigned char character = (unsigned char)text.array[ in ];
				switch( character ){
					case '"':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = '"';	break;
					case '\\':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = '\\';	break;
					case '\n':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = 'n';	break;
					case '\r':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = 'r';	break;
					case '\t':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = 't';	break;
					case '\b':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = 'b';	break;
					case '\f':	escaped.array[ out++ ] = '\\'; escaped.array[ out++ ] = 'f';	break;
					default:
						if( character < 0x20 ){
							out = out + (uint16_t)snprintf( &escaped.array[ out ], 7, "\\u%04x", character );
						}else{
							escaped.array[ out++ ] = (char)character;
						}
				}
			}
			escaped.array[ out ] = 0;
			return escaped;
		}
		/*	snprintf reports what it wanted to write and not what it wrote, so a body that did not
			fit comes back as 0 instead of as a truncated json document. */
		static uint32_t written( int result, uint32_t size ){
			return ( ( result > 0 ) && ( (uint32_t)result < size ) ) ? (uint32_t)result : 0;
		}
		static uint32_t message_body( char* out, uint32_t size, const char* username, struct string content ){
			const struct string text = escape( content );
			if( username ){
				const struct string name = escape( str( username ) );
				return written( snprintf( out, size, "{\"username\":\"%s\",\"content\":\"%s\"}", name.array, text.array ), size );
			}
			return written( snprintf( out, size, "{\"content\":\"%s\"}", text.array ), size );
		}
		static uint32_t embed_body( char* out, uint32_t size, struct discord_embed embed ){
			const struct string title = escape( str( embed.title ? embed.title : "" ) );
			const struct string description = escape( str( embed.description ? embed.description : "" ) );
			return written(
				snprintf(
					out, size,
					"{\"embeds\":[{\"title\":\"%s\",\"description\":\"%s\",\"color\":%u}]}",
					title.array, description.array, embed.color
				),
				size
			);
		}
		/*	discord answers a 429 with the wait in seconds, as a fraction when it is below one */
		static uint32_t retry_after( HINTERNET request ){
			wchar_t value[ 32 ] = {0};
			ULONG size = sizeof(value);
			if( ! winhttp.WinHttpQueryHeaders( request, WINHTTP_QUERY_CUSTOM, L"Retry-After", value, &size, 0 ) ){
				return 0;
			}
			return (uint32_t)( wcstod( value, 0 ) * 1000.0 );
		}
		/*	one POST. The connection and the request handle live for this call only, the session
			handle behind them belongs to the channel and stays open, so the next message reuses
			the resolved name and the tls session instead of shaking hands again. */
		static struct discord_response request( struct channel* this, const char* body, uint32_t length ){
			struct discord_response response = {0};
			if( ( this == 0 ) || ( this->session == 0 ) || ( length == 0 ) ){ return response; }
			HINTERNET connection = winhttp.WinHttpConnect( this->session, this->host, this->port, 0 );
			if( connection == 0 ){
				lasterror = GetLastError();
				return response;
			}
			HINTERNET post = winhttp.WinHttpOpenRequest(
				connection, L"POST", this->path, 0, 0, 0, this->secure ? WINHTTP_FLAG_SECURE : 0
			);
			if( post == 0 ){
				lasterror = GetLastError();
				winhttp.WinHttpCloseHandle( connection );
				return response;
			}
			if( winhttp.WinHttpSendRequest( post, this->headers, (ULONG)-1, (void*)body, length, length, 0 )
				&& winhttp.WinHttpReceiveResponse( post, 0 ) ){
				ULONG status = 0;
				ULONG statussize = sizeof(status);
				if( winhttp.WinHttpQueryHeaders( post, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 0, &status, &statussize, 0 ) ){
					response.status = (uint16_t)status;
				}
				if( response.status == 429 ){
					response.retry_after = retry_after( post );
				}
				ULONG read = 0;
				if( winhttp.WinHttpReadData( post, response.body.array, ARRSIZE - 1, &read ) ){
					response.body.array[ read ] = 0;	/*	whatever is left on the handle goes with it */
				}
			}else{
				lasterror = GetLastError();
			}
			winhttp.WinHttpCloseHandle( post );
			winhttp.WinHttpCloseHandle( connection );
			return response;
		}
		/*	being rate limited is normal and not an error- discord says how long to wait and the
			second attempt is the one that lands. It waits once, and only for a wait short enough
			to block a tick on, anything longer is the callers to decide on from retry_after. */
		static struct discord_response post_with_retry( struct channel* this, const char* body, uint32_t length ){
			struct discord_response response = request( this, body, length );
			if( ( response.status == 429 ) && ( response.retry_after != 0 ) && ( response.retry_after <= 5000 ) ){
				Sleep( response.retry_after );
				response = request( this, body, length );
			}
			return response;
		}
		/*	the first parameter is the union converter and not a struct channel*, so the assignment
			in new() lines up with the interfaces struct discord_interface* without a cast
			anywhere- and inside the function the private members are reachable as this.channel. */
		static struct discord_response send( union converter this, struct string content ){
			char body[ BODYSIZE ];
			const uint32_t length = this.channel
				? message_body( body, sizeof(body), this.channel->username, content )
				: 0;
			return post_with_retry( this.channel, body, length );
		}
		static struct discord_response post( union converter this, struct discord_embed embed ){
			char body[ BODYSIZE ];
			const uint32_t length = this.channel ? embed_body( body, sizeof(body), embed ) : 0;
			return post_with_retry( this.channel, body, length );
		}
		static void close( union converter this ){
			if( this.channel == 0 ){ return; }
			if( this.channel->session ){ winhttp.WinHttpCloseHandle( this.channel->session ); }
			free( this.channel );
		}
		static struct discord_interface* new( struct discord_arguments arguments ){
			if( ! bind_winhttp() ){ return 0; }
			struct channel* newchannel = calloc( sizeof(struct channel), 1 );
			if( newchannel == 0 ){ return 0; }
			newchannel->username = arguments.username;
			bool addressed = false;
			if( arguments.webhook ){
				addressed = split_url( arguments.webhook, newchannel );
				wcscpy( newchannel->headers, L"Content-Type: application/json" );
			}else if( arguments.token && arguments.channel ){
				char url[ 256 ];
				addressed = written( snprintf( url, sizeof(url), "https://discord.com/api/v10/channels/%s/messages", arguments.channel ), sizeof(url) )
							&& split_url( url, newchannel );
				/*	the token is the whole credential, so it goes into the header and nowhere
					else- it is not kept on the channel and it never reaches a log line. */
				const struct wstring token = wstring( str( arguments.token ) );
				addressed = addressed && ( swprintf(
					newchannel->headers, ARRAYSIZE(newchannel->headers),
					L"Content-Type: application/json\r\nAuthorization: Bot %ls", token.array
				) > 0 );
			}
			if( ! addressed ){
				free( newchannel );
				return 0;
			}
			/*	one session per channel, opened here so a caller holding an interface knows the
				url parsed and winhttp is up- the only failure left for the first message to
				discover is the networks. */
			newchannel->session = winhttp.WinHttpOpen( L"coope/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0 );
			if( newchannel->session == 0 ){
				lasterror = GetLastError();
				free( newchannel );
				return 0;
			}
			if( arguments.timeout ){
				winhttp.WinHttpSetTimeouts(
					newchannel->session,
					(int)arguments.timeout, (int)arguments.timeout, (int)arguments.timeout, (int)arguments.timeout
				);
			}
			newchannel->send = &send;
			newchannel->post = &post;
			newchannel->close = &close;
			return &newchannel->discord_interface;
		}
		static unsigned long last_error( void ){
			return lasterror;
		}
		typeof(discord) discord = { .new = new, .escape = escape, .last_error = last_error };
#	endif
#endif