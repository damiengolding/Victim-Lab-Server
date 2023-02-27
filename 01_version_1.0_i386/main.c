#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

#ifdef WIN32
	typedef int socklen_t;
	#include <windows.h>
	#include <winbase.h>
	#include <errno.h>
	#include <winsock2.h>
#else
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <errno.h>
#endif

unsigned short int _listenPort;
int _svrListenSocket, _svrConnectSocket;
struct sockaddr_in clientAddress, _serverAddress;
socklen_t _clientAddressLength;
struct sockaddr _clientAddress;

#define VER_STR "Victim Lab Server 1.1\r\n"
#define MAX_BUF 65535

/*
* Accepts an incoming connection and returns the created socket descriptor
*/

int acceptConn(){
	while( TRUE ){
		_clientAddressLength = sizeof( _clientAddress );
		_svrConnectSocket = accept( _svrListenSocket, ( struct sockaddr * ) &_clientAddress, &_clientAddressLength );
		if ( _svrConnectSocket < 0 ) {
			continue;
		}
		else{
			return( _svrConnectSocket );
		}
    }	
    return( _svrConnectSocket );
}

/*
* Instantiates the server socket and calls listen 
*/

int startServer( int p ){
	_listenPort = p;
    char b[10];
    itoa( p, b, 10 );
    
	WSADATA wsaData = {0};
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        wprintf(L"WSAStartup failed: %d\n", iResult);
        return 1;
    }
    _svrListenSocket = socket( AF_INET, SOCK_STREAM, 0 );
    if( _svrListenSocket < 0 ){
		printf( "Failed to open socket on port %s\r\n", b );
		printf( "Last eror: %", WSAGetLastError() );
		return ( -2 );
	}
	_serverAddress.sin_family = AF_INET;
	_serverAddress.sin_addr.s_addr = htonl( INADDR_ANY );
	_serverAddress.sin_port = htons( _listenPort );
	if ( bind ( _svrListenSocket, ( struct sockaddr * ) &_serverAddress, sizeof ( _serverAddress ) ) == -1 ) {
		printf( "Failed to bind: %d", WSAGetLastError() );	
	}
	else{
		listen( _svrListenSocket, 10 );
	}
    
    return( 0 );   
}

/*
* Graceful shutdown
*/

int close(){
	closesocket( _svrListenSocket );	
	WSACleanup();
	return( 0 );
}

/*
* Clean up function
*/

void cleanup( void* arg ){
	// Cleanup;
}

/*
* Evaluation function
*/

void evaluate( char *ev, char *out, int max ){
	char *buf = (char*) malloc( sizeof( char*) * 1024 );
	char *rl = (char*) malloc( sizeof( char*) * 1024 );
	char *prog = ev;
	strcat( prog, " 2>&1" );
	FILE *proc;
	if(  proc = (FILE*) popen ( (const char*)prog, "r" ) ) {
		while( fgets( buf, 1024, proc ) ){
			strcat( rl, buf );	
		} 
	}
	else{
		printf( "proc==NULL\r\n" );	
	}
	strncpy(out,rl,max);
	pclose(proc);
}

/*
* Thread func to handle the incoming socket
*/

void* handleConn( void* arg ){
/*
* Main connection handling
*/

	int s = (intptr_t)arg;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    //pthread_cleanup_push( cleanup, arg );
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
	int sent = send( s, VER_STR, strlen( VER_STR ) + 1, 0 );
	int blen = 1024;
	char buf[blen];
	char eval[56];
	int ret;
	do{
		ret = recv( s, buf, blen, 0 );
		if( ret == -1 ){
			printf( "Error number: %d\r\n", errno );
			printf( "Error string: %d\r\n", WSAGetLastError() );
		}
		if( strncmp( buf, "QUIT", 4 ) == 0 || strncmp( buf, "quit", 4 ) == 0 ){
			send( s, "Quitting\r\n", strlen( "Quitting\r\n") + 1, 0 );
			break;
		}
		else if( strncmp( buf, "HELO", 4 ) == 0 || strncmp( buf, "helo", 4 ) == 0 ){
			send( s, "Ready\r\n", strlen( "Ready\r\n") + 1, 0 );
		}
		else if( strncmp( buf, "SEND", 4 ) == 0 || strncmp( buf, "send", 4 ) == 0 ){
			DWORD len = 1024;
			char cname[len], usname[len], hn[len];
			char *ip = (char*)malloc( sizeof( char*) * len );
			len = 1024;
			GetComputerName( cname, &len );
			len = 1024;
			GetUserName( usname, &len );
			len = 1024;
			gethostname( hn, 1024 );
			hostent *hst = gethostbyname( hn );
			struct in_addr addr;
			addr.s_addr = *(u_long*)hst->h_addr_list[0];
			ip = inet_ntoa( addr );
			char sbuf[len];
			sprintf( sbuf, "computer=%s\r\nuser=%s\r\nhost=%s\r\nip=%s\r\n", cname, usname, hn, ip );
			
			send( s, sbuf, strlen( sbuf ) + 1, 0 );
		}
		else{
			send( s, "Talk sense, man\r\n", strlen( "Talk sense, man\r\n") + 1, 0 );
			strcpy(eval,buf);
			char *po = (char*)malloc( sizeof( char* ) * blen );
			evaluate(eval,po,blen);
			send( s, po, blen, 0 );
		}
	} while ( ret > 0 );
	//pthread_cleanup_pop(1);
	shutdown( s, SD_SEND );
	return(arg);
/*
* End of connection handling
*/
}
	
/*
* Helper function to instantiate the p_thread
*/

int run( int s ){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t _the_thread;
	int ret;
	ret = pthread_create ( &_the_thread, &attr, handleConn, (void*)(intptr_t)s );
	if( ret != 0 ){
		return(0);
	}	
	return(1);
}

/*
* Main: vlab*_*.exe <port>
*/

int main(int argc, char *argv[]){
	int port=666;
	printf( VER_STR );
	if( argc >= 2 ){
		port = atoi( argv[1] );
	}
	startServer( port );
	printf( "Accepting incoming connections on port: %d\r\n", port );
	while( TRUE ){
		run( acceptConn() );
	}
	exit( 0 );
}
 
