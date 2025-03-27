#include <unistd.h>
#include <errno.h>
#include "../inc/lines.h"
#include <arpa/inet.h>

int sendMessage(int socket, char * buffer, int len)
{
	int r;
	int l = len;
		

	do {	
		r = write(socket, buffer, l);
		l = l -r;
		buffer = buffer + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fail */
	else
		return(0);	/* full length has been sent */
}

int recvMessage(int socket, char *buffer, int len)
{
	int r;
	int l = len;
		

	do {	
		r = read(socket, buffer, l);
		l = l -r ;
		buffer = buffer + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fallo */
	else
		return(0);	/* full length has been receive */
}



ssize_t readLine(int fd, void *buffer, size_t n)
{
	ssize_t numRead;  /* num of bytes fetched by last read() */
	size_t totRead;	  /* total bytes read so far */
	char *buf;
	char ch;


	if (n <= 0 || buffer == NULL) { 
		errno = EINVAL;
		return -1; 
	}

	buf = buffer;
	totRead = 0;
	
	for (;;) {
        	numRead = read(fd, &ch, 1);	/* read a byte */

        	if (numRead == -1) {	
            		if (errno == EINTR)	/* interrupted -> restart read() */
                		continue;
            	else
			return -1;		/* some other error */
        	} else if (numRead == 0) {	/* EOF */
            		if (totRead == 0)	/* no byres read; return 0 */
                		return 0;
			else
                		break;
        	} else {			/* numRead must be 1 if we get here*/
            		if (ch == '\n')
                		break;
            		if (ch == '\0')
                		break;
            		if (totRead < n - 1) {		/* discard > (n-1) bytes */
				totRead++;
				*buf++ = ch; 
			}
		} 
	}
	
	*buf = '\0';
    	return totRead;
}

void send_double(int socket, double d) {
	// Convertir el double a network byte order
    long int i64 = *((long int *)(&d));
    int hiword = htonl((int)(i64 >> 32));
    sendMessage(socket, (char *)&hiword, sizeof(hiword));
    int loword = htonl((int)(i64));
    sendMessage(socket, (char *)&loword, sizeof(loword));
}

double recv_double(int socket) {
	// Recibir los dos enteros de 32 bits en network byte order y convertirlos a double
    int hiword, loword;
    recvMessage(socket, (char *)&hiword, sizeof(hiword));
    recvMessage(socket, (char *)&loword, sizeof(loword));
    hiword = ntohl(hiword);
    loword = ntohl(loword);
    long int i64 = (((long int) hiword) << 32) | loword;
    return *((double *)(&i64));
}
