/* -------------------------------------------------------- *
assault.c
------

Summary
-------
A test suite to test socket delivery rates against web servers. 

Requirements
------------
- libcurl
- libpng
- pthread

Usage
-----
Options for using `assault` are as follows:
<pre>
-c, --count <arg>      Run tests this many times.
-u, --url <arg>        Run against this URL.
-m, --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )
-d, --directory <arg>  Store the results here.
-x, --discard          No data will be kept
    --dry-run          Show what we would have done.
</pre>

 * -------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#ifndef		M_PI
#define		M_PI		3.14159265358979323846264338
#endif

#define PROGRAM "assault: "

int opt_dryrun = 0;
int opt_memory = 0;
int opt_throwaway = 0;
int opt_binary = 0;
int rcount = 1;
int body_size = 1;
char path[ 2048 ] = { 0 }; 
char url[ 2048 ] = { 0 }; 
char method[ 16 ] = { 'G', 'E', 'T', '\0' };
const char *methods[6] = { "HEAD", "GET", "POST", "PATCH", "PUT", "DELETE" };


typedef struct HttpResult {

	//Content length of the request
	int length;

	//Status of the request
	unsigned int status;

	//Unix time stamps for start and stop time
	unsigned int time_started, time_completed;

	//Response data (why not unsigned again?)
	char *content;

	//Other parts of the request
	int hin_size;
	int sslin_size;
	char *dout;
	char *hin, *hout;
	char *sslin, *sslout;
	char *info;

	//Thread info
	pthread_t thread_id;
	int index;


	char error[ 128 ];
} HttpResult;



//Show help
void help() {
	fprintf( stderr, "-c, --count <arg>      Run tests this many times.\n" );
	fprintf( stderr, "-u, --url <arg>        Run against this URL.\n" );
	fprintf( stderr, "    --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )\n" );
	fprintf( stderr, "-d, --directory <arg>  Store the results here.\n" );
	fprintf( stderr, "-x, --discard          Discard any results from server.\n" );
	fprintf( stderr, "    --dry-run          Show what we would have done.\n" );
	fprintf( stderr, "-m, --method <arg>     Use <type> of method when making a request\n" );
	fprintf( stderr, "                       (Available options are GET, POST, PATCH, PUT, DELETE, HEAD)\n" );
}



//Copy arguments
char * copy_arg ( char **av, char **dest, int len ) {
	char *arg = *av;

	if ( !( *( ++av ) ) || **av == '-' ) {
		fprintf( stderr, PROGRAM "Expected argument, received flag at %s.\n", arg );
		return NULL;
	}

	memset( dest, 0, len );
	memcpy( dest, *av, strlen( *av ) );
	return *dest;
}




//Return a blank buffer for multiple fields
char *allocbuf ( char *data, size_t size ) {
	char *d = malloc( size + 4 );
	memset( d, 0, size + 1 );
	memcpy( d, data, size );
	return d; 
}



//Special callback to populate with all needed data
int db( CURL *h, curl_infotype type, char *data, size_t size, void *ptr ) {

	HttpResult *r = (HttpResult *)ptr;
	switch (type) {
		case CURLINFO_TEXT:
			r->info = allocbuf( data, size );
		default: /* in case a new one is introduced to shock us */
			return 0;
		case CURLINFO_HEADER_OUT:
			r->hout = allocbuf( data, size );
			break;
		case CURLINFO_DATA_OUT:
			r->dout = allocbuf( data, size );
			break;
		case CURLINFO_SSL_DATA_OUT:
			r->sslout = allocbuf( data, size );
			break;
		case CURLINFO_HEADER_IN:
			r->hin = allocbuf( data, size ); 
			r->hin_size = size;
			break;
		case CURLINFO_SSL_DATA_IN:
			r->sslin = allocbuf( data, size ); 
			r->sslin_size = size;
			break;
		case CURLINFO_DATA_IN:
			r->content = allocbuf( data, size ); 
			r->length = size;
			break;
  }

	return 0;
}


//Generate random WAV data of an arbitrary length (under 4gb, please)
int * generate_random_wav ( unsigned int length, unsigned int *size ) {
	
	//Calculate constants here
	const int bits_per_sample = 16;
	const int channels = 2;
	const int sample_rate = 44100;
	const int sample_count = sample_rate * length;
	const int byterate = sample_rate * channels * ( bits_per_sample / 8 );
	const float vol = 32000.0;
	int *buffer = NULL;
	unsigned int bufsize = 0;

	//Catch useless lengths
	if ( length < 1 ) {
		fprintf( stderr, "Length must be longer than 1 second.\n" );
		return NULL;
	}

	if ( length * byterate > INT_MAX ) {
		fprintf( stderr, "Length is too long.\n" );
		return NULL;
	}

	//Simple RIFF header, courtesy of: http://soundfile.sapp.org/doc/WaveFormat/
	unsigned int header[12] = {
		0x46464952 // 'RIFF'
	, 36 + ( sample_rate * length ) * channels * ( bits_per_sample / 8 )
	, 0x45564157 // 'WAVE'
	, 0x20746d66 // 'fmt '
	, 0x00000010 // PCM
	, 0x00020001 // Use PCM audio format & specify single channel
	, sample_rate // Sample rate (44100)
	, byterate // byte rate ( 44100 * # of channels * bits per sample/8 )
	, 0x00100004 // block alignment and bits per sampl
	, 0x61746164 // 'data'
	, ( sample_rate * length ) * channels * ( bits_per_sample / 8 )
	, 0x00000000
	};	

	//Define buffer size
	bufsize = 2 * ( channels * sample_count ) * sizeof( int ); 

	//Allocate and die if we're out of space
	if ( !( buffer = malloc( 44 + bufsize ) ) ) {
		fprintf( stderr, "Failed to generate buffer for sound data.\n" );
		return 0;
	}

	//Write the header
	memset( buffer, 0, 44 + bufsize );
	memcpy( buffer, header, 44 );

	//Generate the sound data
	buffer += 44;
	for ( int i = 0; i < sample_count; i++ ) {
		buffer[ ( 2 * i ) ] = vol * sin( ( 440.0 / sample_rate ) * 2 * i * M_PI ); 
		buffer[ ( 2 * i ) + 1 ] = vol * sin( ( 600.0 / sample_rate ) * 2 * i * M_PI ); 
	}

	//Get fancy :)
	buffer -= 44;
	*size = 44 + bufsize; 
	return buffer;
}


//Destroy httpResult
void free_http_result ( HttpResult *r ) {
	free( r->dout );
	free( r->hin );
	free( r->hout );
	free( r->sslin );
	free( r->sslout );
	free( r->info );
	free( r->content );
}


//Destroy httpResult
void analyze_http_result ( HttpResult *r ) {
	fprintf( stderr, "Request #%d\n", r->index);
	//fprintf( stderr, "INFO:\n%s\n",r->info );
	fprintf( stderr, "Header sent by us:\n%s\n", r->hout );
	fprintf( stderr, "Header received:\n%s\n", r->hin );
	//fprintf( stderr, "%s\n",r->sslin );
	//fprintf( stderr, "%s\n",r->sslout );
	fprintf( stderr, "Data sent by us:\n%s\n",r->dout );
	fprintf( stderr, "Data received (%d bytes):\n", r->length );
	//write( 2, r->content, 10 );
}



//Generate random strings to test out my understanding of pthreads
char * generate_random_string () {
	char *content = malloc( 11 );
	memset( content, 0, 11 );

	for ( int i = 0; i < 10; i++ ) {
		content[ i ] = ( rand() % 93 ) + 32;
	}
	
	content[ 10 ] = '\0';
	return content;
}


//Generate a body if necessary
int generate_body ( CURL *handle, void *p ) {
	if ( !strcmp( "HEAD", method ) || !strcmp( "GET", method ) !! strcmp( "DELETE", method ) ) {
		return 0;
	}

	//Binary, JSON, XML or not?
	if ( !opt_binary ) {
		//Generate a regular POST request
	}

	return 1;
}



//Generate a request to a server
void * make_request ( void *arg ) {
	HttpResult *r = ( HttpResult * )arg; 
	FILE *f = NULL;
	CURLcode res;
	CURL *h = curl_easy_init();

	if ( !h ) {
		//snprintf( r->error, 128, "CURL failed to initialize at thread %s.\n", r->thread_id );
		return NULL;
	}

	//Initialize HttpResult
	r->length = 0;
	r->content = NULL;
	r->dout = NULL;
	r->hin = NULL;
	r->hout = NULL;
	r->sslin = NULL;
	r->sslout = NULL;
	r->info = NULL;

	//Set the URL
	curl_easy_setopt( h, CURLOPT_URL, url );

	//TODO: Bring CURL_MAX_WRITE_SIZE up to test larger files
	//Always be verbose b/c we need a lot of info
	curl_easy_setopt( h, CURLOPT_VERBOSE, 1 );
	curl_easy_setopt( h, CURLOPT_HEADER, 1 );
	curl_easy_setopt( h, CURLOPT_DEBUGFUNCTION, db );
	curl_easy_setopt( h, CURLOPT_DEBUGDATA, r );

	//Set the method and any data
	

	//Choose whether it's going to memory or not
	if ( opt_memory ) {
		;
	}
	else {
		//Open a blank device (we don't really want to store the data)
		f = fopen( "/dev/null", "w" );

		//Set data to file handle
		curl_easy_setopt( h, CURLOPT_WRITEDATA, f );
		curl_easy_setopt( h, CURLOPT_STDERR, f );

		//Perform the request
		if ( ( res = curl_easy_perform( h ) ) != 0 ) {
			//Set r->error here
		}

		fclose( f );	
	}

	//Clean up
	curl_easy_cleanup( h );
	return r;	
}





int main ( int argc, char *argv[] ) {

	if ( argc < 2 ) {
		fprintf( stderr, PROGRAM "no options specified.\n" );
		help();
		return 1;
	}

	//Get all the options
	while ( *( ++argv ) ) {
		if ( !strcmp( *argv, "-c" ) || !strcmp( *argv, "--count" ) )
			rcount = atoi( *( ++argv ) );
		else if ( !strcmp( *argv, "-s" ) || !strcmp( *argv, "--size" ) )
			body_size = atoi( *( ++argv ) );
		else if ( !strcmp( *argv, "-b" ) || !strcmp( *argv, "--binary" ) )
			opt_binary = 1; 
		else if ( !strcmp( *argv, "--dry-run" ) )
			opt_dryrun = 1;	
		else if ( !strcmp( *argv, "--memory" ) )
			opt_memory = 1;	
		else if ( !strcmp( *argv, "-x" ) || !strcmp( *argv, "--discard" ) )
			opt_throwaway = 1;	
		else if ( !strcmp( *argv, "-u" ) || !strcmp( *argv, "--url" ) ) {
			int found = 0;
			if ( !copy_arg( argv, ( char ** )&url, sizeof(url) ) ) {
				return 1; 
			}
			//check that the url is valid in some way
			if ( strlen( url ) < 8 || ( memcmp( url, "http://", 7 ) && memcmp( url, "https://", 8 ) ) ) {
				fprintf( stderr, PROGRAM "This doesn't look like a valid URL '%s'.\n", url );
				return 1;
			}
			argv++;
		}
		else if ( !strcmp( *argv, "-d" ) || !strcmp( *argv, "--directory" ) ) {
			if ( !copy_arg( argv, ( char ** )&path, sizeof(path) ) ) 
			return 1; 
			argv++;
		}

		else if ( !strcmp( *argv, "-m" ) || !strcmp( *argv, "--method" ) ) {
			int found = 0;
			if ( !copy_arg( argv, ( char ** )&method , sizeof(method) ) ) {
				return 1; 
			}
			for ( int i = 0; i < 6; i++ ) {
				if ( !strcasecmp( method, methods[ i ] ) ) {
					found = 1;
					break;	
				}
			}
			if ( !found ) {
				fprintf( stderr, PROGRAM "Unsupported HTTP method '%s'.\n", method );
				return 1;
			}
			argv++;
		}
		else {
			fprintf( stderr, PROGRAM "Unsupported option %s.\n", *argv );
			return 1;
		}
	}

	
	//Do some sanity checks
	if ( rcount < 1 ) {
		fprintf( stderr, PROGRAM "Value given to --count cannot be less than 1.\n" );
		return 1;
	}

	if ( !( *url ) ) {
		fprintf( stderr, PROGRAM "No URL specified.\n" );
		return 1;
	}

	if ( !opt_throwaway && !opt_memory && !( *path ) ) {
		fprintf( stderr, PROGRAM "None of the following flags were specified: --discard, --directory or --memory.  I have nowhere to save my findings...\n" );
		return 1;
	}

	//Is this a dry run
	if ( opt_dryrun ) {
		fprintf( stderr, "rcount: %d\n", rcount );	
		fprintf( stderr, "path:   %s\n", !( *path ) ? "" : path );	
		fprintf( stderr, "url:    %s\n", !( *url ) ? "" : url );	
		return 1;
	}	
	
	//Setup threading
	pthread_attr_t attr;
	int s = pthread_attr_init( &attr );
	if ( s != 0 ) {
		fprintf( stderr, PROGRAM "Error at pthread_attr_init: %s.", strerror( errno ) );
		return 1;
	}

	//Allocate enough memory upfront for all the requests
	HttpResult *results = malloc( sizeof( HttpResult ) * rcount );
	if ( !results ) {
		fprintf( stderr, PROGRAM "Could not allocate enough space for results." );
		return 1;
	}

	//Run all of them
	for ( int i = 0; i < rcount ; i ++ ) {
		results[ i ].index = i;
		s = pthread_create( &results[ i ].thread_id, &attr, make_request, &results[ i ] );		
	}

	//Destroy original structure for creating the threads.
	if ( ( s = pthread_attr_destroy( &attr ) ) != 0 ) {
		fprintf( stderr, PROGRAM "Problems deallocating pthread structure: %s.", strerror( errno ) );
		return 1;
	}

	//Activate cURL
	curl_global_init( CURL_GLOBAL_ALL );

	//If they're all finished let's do some stuff
	for ( int i = 0; i < rcount ; i ++ ) {
		HttpResult *rr = NULL;

		if ( ( s = pthread_join( results[ i ].thread_id, (void **)&rr ) ) != 0 ) {
			fprintf( stderr, PROGRAM "Something went wrong with pthread_join: %s.", strerror( errno ) );
			return 1;
		}

		//...
		fprintf( stderr, "Joined with thread %d\n", i );

		//What exactly is here?
		analyze_http_result( rr );	

		//Destroy the HTTP result	
		free_http_result( rr );	
	}

	//Destory the array
	free( results );

	//Turn off cURL
	curl_global_cleanup();	
	return 0;	
}
