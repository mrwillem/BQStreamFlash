/* Diese Datei ist Teil von BQ34StreamFlash.
 *
 * BQ34StreamFlash ist Freie Software: Sie können es unter den Bedingungen
 * der GNU General Public License, wie von der Free Software Foundation,
 * Version 3 der Lizenz oder (nach Ihrer Wahl) jeder späteren
 * veröffentlichten Version, weiterverbreiten und/oder modifizieren.
 *
 * BQ34StreamFlash wird in der Hoffnung, dass es nützlich sein wird, aber
 * OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
 * Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 * Siehe die GNU General Public License für weitere Details.
 *
 * Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
 * Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "serial.h"





volatile int STOP = 0;
int send_program_stream(int termfd, uint8_t *data)
{
	write(termfd, data, 64);
	return decode_serial_answer(termfd);
}

int decode_serial_answer(int termfd)
{
	STOP=FALSE;
	int res;
	char buf[64];
	memset(buf,0,64);
	while (STOP==FALSE)
	{
		/* loop for input */
		res = read(termfd,buf,255);   /* returns after 10 chars have been input */
		buf[res]=0;               /* so we can printf... */
		printf(":%s:%d\n", buf, res);
		if (strncmp(buf, "cmpfail", 7) == 0)
		{
			return CMPFAIL;
		}
		else if(strncmp(buf, "next_pack", 9) == 0)
		{
			return NEXTPACK;
		}
		else if(strncmp(buf, "ackno", 5) == 0)
		{
			return ACKNOL;
		}
		else if(strncmp(buf, "resend", 6) == 0)
		{
			return RESEND;
		}
		else if(strncmp(buf, "fullresend", 10) == 0)
		{
			return FULLRESEND;
		}
		else if(strncmp(buf, "exit_programm_mode", 18) == 0)
		{
			return EXIT_PROGMODE;
		}
	}
	return 0;
}

void send_programming_mode(int termfd, int * mode)
{
	char * buf;
	int res;
	int errorcount;
	errorcount=0;
	buf = (char *) malloc(20*sizeof(char));
	buf=strncpy(buf, "set_bq34_rommode", 16);
	buf[16]=0x0d;
	buf[17]=0x0a;
	printf(" Writing %s to %i\n", buf, termfd);
	write(termfd, buf, 18);
	memset(buf,0,20);
	while (STOP==FALSE)
	{
		/* loop for input */
		res = read(termfd,buf,255);   /* returns after 10 chars have been input */
		buf[res]=0;               /* so we can printf... */
		printf(":%s:%d\n", buf, res);

		if (strncmp(buf, "bq34_rommode_set", 16) == 0)
		{
			*mode=2;
			STOP=TRUE;
		}
		else if((strncmp(buf, "Invalid", 7) == 0) || (strncmp(buf, "reset_bqtx", 10) == 0))
		{
			if(errorcount < 10)
			{
				errorcount++;
				buf=strncpy(buf, "set_bq34_rommode", 16);
				buf[16]=0x0d;
				buf[17]=0x0a;
				write(termfd, buf, 18);
				memset(buf,0,20);
			}
			else
			{
				*mode=1;
				STOP=TRUE;
			}
		}
	}
	free(buf);
}

int serial_open( int *termfd, char *device)
{
	*termfd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
	if (*termfd <0)
	{
		perror(device);
		return -1;
	}
	return 0;
}

void serial_close( int *termfd)
{
	// tcsetattr(termfp,TCSANOW,&oldtio);
	close(*termfd);
}


int serial_set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                perror("serial_set_interface_attribs");
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                perror("serial_set_interface_attribs");
                return -1;
        }
        return 0;
}

int serial_set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                perror("tcgetattr");
                return -1;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
	{
                perror("tcsetattr");
		return -1;
	}
	return 0;
}
