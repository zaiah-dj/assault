/* -------------------------------------------------------- *
assault.c
------

Summary
-------
A test suite to test socket delivery rates against web servers. 

Requirements
------------
- libcurl
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
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#define PROGRAM "assault: "

int opt_dryrun = 0;
int opt_memory = 0;
int opt_throwaway = 0;
int rcount = 1;
char path[ 2048 ] = { 0 }; 
char url[ 2048 ] = { 0 }; 
char type[ 8 ] = { 'G', 'E', 'T' };
char *types[6] = { "HEAD", "GET", "POST", "PATCH", "PUT", "DELETE" };

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
	fprintf( stderr, "-e, --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )\n" );
	fprintf( stderr, "-d, --directory <arg>  Store the results here.\n" );
	fprintf( stderr, "-x, --discard          Discard any results from server.\n" );
	fprintf( stderr, "    --dry-run          Show what we would have done.\n" );
	fprintf( stderr, "-m, --method <arg>     Use <type> of method when making a request\n" );
	fprintf( stderr, "                       (Available options are GET, POST, PATCH, PUT, DELETE, HEAD)\n" );
}



//Copy arguments
char * copy_arg ( char **av, char **dest ) {
	char *arg = *av;

	if ( !( *( ++av ) ) || **av == '-' ) {
		fprintf( stderr, PROGRAM "Expected argument, received flag at %s.\n", arg );
		return NULL;
	}

	memset( dest, 0, strlen( *av ) + 5 );
	memcpy( dest, *av, strlen( *av ) );
	return *dest;
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
	while ( *argv ) {
		if ( !strcmp( *argv, "-c" ) || !strcmp( *argv, "--count" ) )
			rcount = atoi( *( ++argv ) );
		else if ( !strcmp( *argv, "--dry-run" ) )
			opt_dryrun = 1;	
		else if ( !strcmp( *argv, "-m" ) || !strcmp( *argv, "--memory" ) )
			opt_memory = 1;	
		else if ( !strcmp( *argv, "-x" ) || !strcmp( *argv, "--discard" ) )
			opt_throwaway = 1;	
		else if ( !strcmp( *argv, "-u" ) || !strcmp( *argv, "--url" ) ) {
			if ( !copy_arg( argv, ( char ** )&url ) ) 
			return 1; 
		}
		else if ( !strcmp( *argv, "-d" ) || !strcmp( *argv, "--directory" ) ) {
			if ( !copy_arg( argv, ( char ** )&path) ) 
			return 1; 
		}
		argv++;
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
