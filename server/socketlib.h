#ifndef _SOCKETLIB_
#define _SOCKETLIB_

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>   //inet_pton
#include <netdb.h>

#define MAXLISTEN 5

/* return socket descriptor to hostname:port
   domain is alway internet: PF_INET 
   type can be: SOCK_STREAM (TCP) | SOCK_DGRAM (UDP)
   
   on error return -1
 */
int connectHost (char *hostname, int port, int sockType) {
int sock;
struct sockaddr_in server;
//struct hostent *hostinfo;

    // Create internet socket 
    sock = socket (PF_INET, sockType, 0);
    if (sock < 0) {
    	perror ("creating socket to host");
    	return -1;	
    	}

    // Define server address
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons (port);       
    if (inet_pton (AF_INET, hostname, &server.sin_addr)<=0) {
    	fprintf (stderr, "Unknown host: %s\n", hostname);
	exit (EXIT_FAILURE);
    }
    
/*  Old way:
	hostinfo = gethostbyname (hostname);
	if (hostinfo == NULL) {
	    fprintf (stderr, "Unknown host: %s\n", hostname);
    	return -1;	
	}
	server.sin_addr = *(struct in_addr *) hostinfo->h_addr;
	server.sin_port = htons (port);
*/
	// Connect to the server
     if (connect (sock, (struct sockaddr *) &server, sizeof (server)) < 0) {
    	perror ("connect to server");
    	return -1;	
    	}

	return sock;
}


//print error message and shutdown connection
void errorSocket(int s, char *m) {
	perror(m);      //print error message
    shutdown(s, 2); // Shutdown full-duplex connction associated with socket s
                    // 2 -> feject future sends and receives 
}


/* create and return a socket descriptor 
   domain is alway internet: PF_INET 
   type can be: SOCK_STREAM (TCP) | SOCK_DGRAM (UDP)
   
   on error return -1
 */
int setSocket (int port, int sockType) {
int sock, n;
struct sockaddr_in server;

    // Create stream internet socket 
	sock = socket (PF_INET, sockType, 0);
	if (sock < 0) {
    	perror ("Unable to create socket!");
    	return -1;	
	}

   	// Enable reutilization of local address 
   	// (n=1 enable option; n=0 disable option)
	n=1;  
   	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n))<0) {
		errorSocket (sock, (char*) "Unable to select socket options");
		return -1;
	}

    // Define server address
    // Define server address
   	memset(&server, 0, sizeof(server));
   	server.sin_family=AF_INET;                   // internet address family
   	server.sin_addr.s_addr=htonl(INADDR_ANY);    // accept any connection 
   	server.sin_port=htons(port);                 // 1023 < port < 65535 
   
   	// bind socket to local server address
   	if (bind(sock, (struct sockaddr *) &server, sizeof(server))<0) {
       	errorSocket (sock, (char*) "Unable to bind to local srver address");
       	return -1;
	}

   // Set socket connection queue for a maximum of MAXLISTEN connections waiting 
   // to be accepted
   if (listen(sock, MAXLISTEN)<0) {
       	errorSocket (sock, (char*) "unable to create connection queue");
		return -1;
	}

	return sock;
}


/* Get first connection from listen queue of server sock
   and return a socket to client
   if no connection available in listen queue return -1
   and deschedule for msec miliseconds
 */  
int getClientSocket(int sock, int msec) {
int csock;
struct sockaddr_in client;
socklen_t lc = sizeof(client);

	/* accept creates a new client socket (csock) with the same properties 
	   of the server socket (sock), and return its descriptor
	   If no connection waiting in queue returns -1
	   In client is stored the client address
     */
    if ((csock = accept (sock, (struct sockaddr *)&client, &lc)) == -1)
		usleep(msec * 1000); //sleep mile seconds

	return csock;
}

#endif
