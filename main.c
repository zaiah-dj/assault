/* -------------------------------------------------------- *
hell.c
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
Options for using `hell` are as follows:
<pre>
-c, --count <arg>      Run tests this many times.
-u, --url <arg>        Run against this URL.
-m, --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )
-d, --directory <arg>  Store the results here.
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

#define PROGRAM "hell: "

int opt_dryrun = 0;
int opt_memory = 0;
int rcount = 1;
char path[ 2048 ] = { 0 }; 
char url[ 2048 ] = { 0 }; 


typedef struct HttpResult {
	int length;
	unsigned int status;
	unsigned int time_started;
	unsigned int time_completed;
	unsigned char *content;
	pthread_t thread_id;
	int index;
} HttpResult;



void help() {
	fprintf( stderr, "-c, --count <arg>      Run tests this many times.\n" );
	fprintf( stderr, "-u, --url <arg>        Run against this URL.\n" );
	fprintf( stderr, "-e, --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )\n" );
	fprintf( stderr, "-d, --directory <arg>  Store the results here.\n" );
	fprintf( stderr, "    --dry-run          Show what we would have done.\n" );
	fprintf( stderr, "-m, --method <arg>     Use <type> of method when making a request\n" );
	fprintf( stderr, "                       (Available options are GET, POST, PATCH, PUT, DELETE, HEAD)\n" );
}



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



char * generate_random_string () {
	char *content = malloc( 11 );
	memset( content, 0, 11 );

	for ( int i = 0; i < 10; i++ ) {
		content[ i ] = ( rand() % 93 ) + 32;
	}
	
	content[ 10 ] = '\0';
	return content;
}


void * make_request ( void *arg ) {
	HttpResult *r = ( HttpResult * )arg; 
	fprintf( stderr, "we do nothing at all with id: %d.", r->index );
	
	r->content = ( unsigned char * )generate_random_string();
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

	if ( !opt_memory && !( *path ) ) {
		fprintf( stderr, PROGRAM "Neither --directory nor --memory flags were specified.  I have nowhere to save my findings...\n" );
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


	//If they're all finished let's do some stuff
	for ( int i = 0; i < rcount ; i ++ ) {
		HttpResult *rr = NULL;

		if ( ( s = pthread_join( results[ i ].thread_id, (void **)&rr ) ) != 0 ) {
			fprintf( stderr, PROGRAM "Something went wrong with pthread_join: %s.", strerror( errno ) );
			return 1;
		}

		fprintf( stderr, "Joined with thread %d, got random content: %s\n", i, (char *)rr->content );
		free( rr->content );
		
	}
	
	return 0;	
}
