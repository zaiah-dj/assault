/* generate wavs? (you can test endless streams fairly easily) */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sndfile.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define	LEFT_FREQ	(344.0 / SAMPLE_RATE)
#define	RIGHT_FREQ (466.0 / SAMPLE_RATE)
#define AMPLITUDE (1.0 * 0x7F000000)
#ifndef		M_PI
#define		M_PI		3.14159265358979323846264338
#endif

#define ZERO_LENGTH
#define LIBSNDFILE_TEST_WAV "sine-lsf.wav"
#define ASSAULT_TEST_WAV "sine-ass.wav"

#if 1
//Using libsndfile
int generate_via_libsndfile() {
	const int length = 4;
	const int amplitude = AMPLITUDE;
	int sample_count = SAMPLE_RATE * length;
	int *buffer = NULL;
	int bufsize = 2 * sample_count * sizeof( int );

	//Set up libsndfile
	SF_VIRTUAL_IO sndfile;
	SF_INFO info;
	SNDFILE * snddata;

	//Allocate and die if we're out of space
	if ( !( buffer = malloc( bufsize ) ) || !memset( buffer, 0, bufsize ) ) {
		fprintf( stderr, "Failed to generate buffer for sound data.\n" );
		return 0;
	}
printf( "sndfile malloc: %d\n", bufsize );

	//Set up the file or audio?
	memset( &info, 0, sizeof( info ) );
	info.samplerate = SAMPLE_RATE;
	info.frames = sample_count;
	info.channels = 2;
	info.format = ( SF_FORMAT_WAV | SF_FORMAT_PCM_16 );	

	//Generate all the data
	for ( int i = 0; i < sample_count; i++ ) {
		buffer[ ( 2 * i ) ] = amplitude * sin( LEFT_FREQ * 2 * i * M_PI ); 
		buffer[ ( 2 * i ) + 1 ] = amplitude * sin( RIGHT_FREQ * 2 * i * M_PI ); 
	}

	SNDFILE *file = NULL; 
	if ( !( file = sf_open( LIBSNDFILE_TEST_WAV, SFM_WRITE, &info ) ) ) {
		fprintf( stderr, "Failed to open sound file to write to\n" );
		return 0;
	}

	if ( sf_write_int( file, buffer, info.channels * sample_count ) != ( info.channels * sample_count ) ) {
		fprintf( stderr, "Failed to write all sound data to file\n" );
		return 0;
	}

	sf_close( file );
	free( buffer );
	return 0;
}
#endif



//Generate random WAV data
int * generate_random_wav ( unsigned int length, unsigned int *size ) {
	
	//Calculate constants here
	const int bits_per_sample = 16;
	const int channels = 2;
	const int sample_rate = 44100;
	const int sample_count = sample_rate * length;
	const int byterate = sample_rate * channels * ( bits_per_sample / 8 );
	const float vol = 32000.0;

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
	int *buffer = NULL;
	unsigned int bufsize = 2 * ( channels * sample_count ) * sizeof( int ); 

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

#if 0
	buffer -= 44;
	int fd = -1;
	//Open a file and write data
	if ( ( fd = open( ASSAULT_TEST_WAV, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU ) ) == -1 ) {
		fprintf( stderr, "Couldn't open wave file for writing: %s\n", strerror( errno ) );	
		return 0;
	}

	if ( write( fd, buffer, 44 + bufsize ) == -1 ) {
		fprintf( stderr, "Couldn't open wave file for writing: %s\n", strerror( errno ) );	
		return 0;
	}

	close( fd );
#endif
	return buffer;
}



#if 0
//Write the file out for test purposes...
int write_file ( unsigned int *a, int size ) {
	//Open a file and write data
	if ( ( fd = open( "yourwav.wav", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU ) ) == -1 ) {
		fprintf( stderr, "Couldn't open wave file for writing: %s\n", strerror( errno ) );	
		return 0;
	}

	if ( write( fd, buffer, bufsize ) == -1 ) {
		fprintf( stderr, "Couldn't open wave file for writing: %s\n", strerror( errno ) );	
		return 0;
	}

	close( fd );
	free( buffer );
	return 1;
}
#endif


#if 1
int main (int argc, char *argv[]) {
	
	int len = 0;
	unsigned int size = 0;
	int *data = NULL;

	if ( argc < 2 ) {
		fprintf( stderr, "I require at least one argument: length in seconds.\n" );
		return 1;
	}


#if 1
	len = atoi( *( ++argv ) );
#else
	while ( *( ++argv ) ) {
		if ( !strcmp( "-l", *argv ) || !strcmp( "--length", *argv ) ) {
			len = atoi( *( ++argv ) ); 			
			return 1;
		}
	}
#endif

	generate_via_libsndfile();

	if ( !(data = generate_random_wav( len, &size )) ) {
		return 1;
	}

#if 0
	if ( !write_file( data, size ) ) {
		return 1;
	} 
#endif

	return 0;
}
#endif
