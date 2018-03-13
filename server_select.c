#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define SERVER_PORT 12345

#define TRUE 1
#define FALSE 0

int main (int argc, char* argv[])
{
	int i, len, rc, on = 1;
	int listen_sd, max_sd, new_sd;
	int desc_ready, end_server = FALSE;
	int close_conn;
	char buffer[80];
	struct sockaddr_in addr;
	struct timeval timeout;
	fd_set master_set, working_set;

	// Create the socket
	listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sd < 0)
	{
		perror("socket() failed");
		exit(-1);
	}
	
	// Allow socket descriptor to be reusable
	rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));	
	if (rc < 0)
	{
		perror("setsockopt() failed");
		close(listen_sd);
		exit(-1);
	}

	// Set socket to be nonblocking.  Sockets for incoming connections will be 
	// nonblocking as well (they will inherit from this one)

	rc = ioctl(listen_sd, FIONBIO, (char *) &on);
	if (rc < 0)
	{
		perror("ioctl() failed");
		close(listen_sd);
		exit(-1);
	}

	// Bind the socket
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);
	rc = bind(listen_sd, (struct sockaddr *) &addr, sizeof(addr));
	if (rc < 0)
	{
		perror("bind() failed");
		close(listen_sd);
		exit(-1);
	}

	// Set the listen back log
	rc = listen(listen_sd, 5);
	if (rc < 0)
	{
		perror("listen() failed");
		close(listen_sd);
		exit(-1);
	}

	// Initialize the master fd_set
	FD_ZERO(&master_set);
	max_sd = listen_sd;
	FD_SET(listen_sd, &master_set);
	
	// Initialize the timeval struct to 3 minutes
	// If there's no activity for three minutes, program will end
	timeout.tv_sec = 3*60;
	timeout.tv_usec = 0;

	// Loop waiting for incoming connections
	do 
	{
		// Copy master fd_set to working fd_set
		memcpy(&working_set, &master_set, sizeof(master_set));
	
		// Call select and wait 3 minutes for it to complete
		printf("Waiting on select()...\n");
		rc = select(max_sd+1, &working_set, NULL, NULL, &timeout);
		if (rc < 0)
		{
			perror(" select() failed");
			break;
		}
		
		if (rc == 0)
		{
			printf(" select() timed out. End program.\n");
			break;
		}

		// If it makes it hear, one or more descriptors are readable
		// Need to determine which are readable
		desc_ready = rc;
		for ( i = 0; i <= max_sd && desc_ready > 0; ++i)
		{
			// Check to see if the descriptor is ready
			if (FD_ISSET(i, &working_set))
			{
				// Found a readable descriptor, so decrease the number
				// of descriptors we're looking for.
				desc_ready -= 1;

				if (i == listen_sd)
				{
					printf(" Listening socket is readable\n");
					// Accept all incoming connections that are 
					// queued up on the listener before we loop back
					// and call select again
					do 
					{
						// Accept each incoming connection. If accept
						// failes with EWOULDBLOCK, then we have accepted 
						// all of them.  Any other failure will cause an exit
						new_sd = accept(listen_sd, NULL, NULL);
						if (new_sd < 0)
						{
							if (errno != EWOULDBLOCK)
							{
								perror("  accept() failed");
								end_server = TRUE;
							}
							break;
						}
						// Add the new incoming connection to the master read set
						printf("  New incoming connection - %d\n", new_sd);
						FD_SET(new_sd, &master_set);
						if (new_sd > max_sd)
							max_sd = new_sd;
						// Loop back up and accept another incoming connection
					} while (new_sd != -1);
				}
				// This is not the listening socket, therefore an existing connection
				// must be readable
				else
				{
					printf(" Descriptor %d is readable\n", i);
					close_conn = FALSE;
					
					// Receive all incoming data before we loop back
					do 
					{
						// Receive data until recv fails with EWOULDBLOCK
						rc = recv(i, buffer, sizeof(buffer), 0);
						if (rc < 0)
						{
							if (errno != EWOULDBLOCK)
							{
								perror("  recv() failed");
								close_conn = TRUE;
							}
							break;
						}
						// Check to see if connection has been closed 
						if (rc == 0)
						{
							printf("  Connection closed.\n");
							close_conn = TRUE;
							break;
						}
						len = rc;
						printf("  %d bytes received\n", len);
						// Echo data back to the client
						rc = send(i, buffer, len, 0);
						if (rc < 0)
						{
							perror("  send() failed");
							close_conn = TRUE;
							break;
						}
					} while (TRUE);
					
					// If the close_conn flag was turned on, we need to clean
					// up this active connection.  This means removing the descriptor
					// from the master set and determining the new max descriptor value
					// based on the bits that are still turned on in the master set

					if (close_conn)
					{
						close(i);
						FD_CLR(i, &master_set);
						if (i == max_sd)
						{
							while (FD_ISSET(max_sd, &master_set) == FALSE)
								max_sd -=1;
						}
					}
				} // End of existing connection is readable


			} // End of if (FD_ISSET(i, &working_set))
		} // End of loop through selectable descriptors
	} while (end_server == FALSE);
		// Clean up all of the sockets that are open
	
	for (i = 0; i <= max_sd; ++i)
	{
		if (FD_ISSET(i, &master_set))
			close(i);
	}
}














