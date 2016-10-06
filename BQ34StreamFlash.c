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
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include "romfile.h"
#include "serial.h"

/*
*for sleep function
*/
#include <time.h>


/*
* A function that sleeps for the given amount of milliseconds 
*/
void sleep_ms(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/*
 * uint8_t *recbuf pointer to line decode receive buffer containing data
 * int txbytes number of bytes of this transfer
 * uint8_t *sertx pointer to serial transmit buffer
 * uint8_t *total_bytes total bytes of this command
 * uint8_t mode command mode either write or compare register
 * uint8_t i2c_slave i2c slave address
 */
void assemble_package(uint8_t *recbuf, int txbytes, uint8_t *sertx, uint8_t total_bytes, uint8_t mode, uint8_t i2c_slave)
{
	uint8_t checksum;
	int i;
	checksum=0xFF;
	/* Write Special Start sequence */
	sertx[0]=0x01;
	sertx[1]=0x5D;
	sertx[2]=0x7D;
	/* Write command =0x01, read command=0x02*/
	sertx[3]=mode;
	/* transmit number of command bytes */
	sertx[4]=total_bytes;
	/* decode the address */
	sertx[5]=i2c_slave;
	for(i=0; i<txbytes; i++)
	{
		sertx[i+6]=recbuf[i];
	}
	while( i < 56)
	{
		sertx[i+6]=0x00;
		i++;
	}
	for(i=0; i<61; i++)
	{
		checksum=checksum -sertx[i];
	}
	/* Package ends with checksum and CR LF */
	sertx[61]=checksum;
	sertx[62]=0x0a;
	sertx[63]=0x0d;
}


int main(int argc, char **argv)
{
	/* Pointer to flashfile */
	FILE * conffp;
	/* file descriptor for the terminal */
	int termfd;
	/* pointer to string to hold one line of the configuration file */
	char * linebuf;
	/* pointer to array that holds the decoded line of the configuration file */
	uint8_t *recbuf;
	/* pointers to array that hold the serial communication data */
	uint8_t *sertx;
	uint8_t *serrx;
	/* variable that holds the length of linebuf, may be changed by readline function */
	size_t len = 1023;
	/* used to store the number of bytes read from file */
	ssize_t read;
	int ret;
	/* variable for the sleep function */
	int sleep;
	int i;
	uint8_t checksum;
	/* file indicators maybe needed to save positions in the current file */
	fpos_t current_command;
	fpos_t flash_stream_start;
	/* some variables to read terminal input */
	int readcharacters;
	char * termbuf;
	size_t termlen = 1023;
	/* defines whether the programming mode is already open */
	int mode;
	mode=0;

	if(argc < 2)
	{
		printf("Please provide serial port on the command line\n");
		return 1;
	}
	else
	{
		if(serial_open(&termfd, argv[1]) == 0)
		{
			if(serial_set_interface_attribs (termfd, BAUDRATE,  0) != 0)
			{
				perror("could not set serial mode");
				return -1;
			}
		}
		else
		{
			perror("could not open serial port");
			return -1;
		}
	}

	/*
	 *Get memory for buffers
	 */
	linebuf=(char *) malloc(1024*sizeof(char));
	termbuf=(char *) malloc(1024*sizeof(char));
	recbuf=(uint8_t *) malloc(129*sizeof(uint8_t));
	serrx=(uint8_t *) malloc(128*sizeof(uint8_t));
	sertx=(uint8_t *) malloc(128*sizeof(uint8_t));
	if((sertx == NULL) || (recbuf == NULL) || (linebuf == NULL) || (serrx == NULL))
	{	
		printf("Error allocating memory for buffers\n");
		return 1;
	}

	conffp = fopen("bq.fs", "r");
	if (conffp == NULL)
	{
		printf("Error opening flash stream file bq.fs\nDoes it exist??\n"); 
 		return 1;
	}
	/*
	 * This sets the MCU into flash programming mode 
	 */
	while(mode != 2)
	{
		send_programming_mode(termfd, &mode);
	}
	/*
	 * Main loop that reads the flash file line by line and sends commands to MCU
	 */
    	do
	{
		ret=decodeline(conffp, linebuf, &len, recbuf);
		switch(ret)
		{
			/* 1 is a Comment line starting with ; */
			case 1:
				break;
			/* 2 is a write command W: */
			case 2:
				/* recbuf holds txbytes */
				/* txbyte[0] is the i2c address txbyte is including the address */
				if(recbuf[128] < 53)
				{
					assemble_package(&recbuf[1],recbuf[128]-1, sertx, recbuf[128]-1,0x01,recbuf[0]);
					ret=send_program_stream(termfd, sertx);
					if(ret !=ACKNOL )
					{
						// RESET
						printf("Failure\n");
					}
		 		}
				else
				{
					assemble_package(&recbuf[1],51,sertx, recbuf[128]-1,0x01, recbuf[0]);
					ret=send_program_stream(termfd, sertx);
					if(ret != NEXTPACK)
					{
						// FAIL
						printf("Failure\n");
					}
					assemble_package(&recbuf[52], recbuf[128]-51, sertx, recbuf[128]-1,0x01, recbuf[0]);
					ret=send_program_stream(termfd, sertx);
					if(ret !=ACKNOL )
					{
						// RESET
						printf("Failure\n");
					}
				}

				break;
			/* 3 is the compare command C: */
			case 3:
				if(recbuf[128] < 52)
				{
					assemble_package(&recbuf[1],recbuf[128]-1, sertx, recbuf[128]-1,0x02, recbuf[0]);
					ret=send_program_stream(termfd, sertx);
					if(ret !=ACKNOL )
					{
						// RESET
						printf("Failure\n What to do now?\nType exit to exit\n");
						readcharacters = getline(&termbuf,&termlen, stdin);
						if(strncmp(termbuf, "exit", 4) == 0)
						{
							printf("You wrote exit\n Will exit now.\n");
							return 1;
						}
					}
				}
				else
				{
					printf("WARNING! Abnormal read command\n Command too long\n");
				}
				break;
			/* 4 is timeout for the given amount of milliseconds */
			case 4:
				sleep=0;
				for(i=1; i<recbuf[0]+1; i++)
				{
					sleep*=10;
					sleep+=recbuf[i];
				}
				printf("Sleeping %i :\n", sleep);
				sleep_ms(sleep);
				break;
			default:
			    strncpy(recbuf, "exit_programm_mode", 18);
				assemble_package(recbuf,18, sertx,18,0x03, 0x00);
				ret=send_program_stream(termfd, sertx);
				if(ret==3)
				{
					ret=-1;
					printf("WTF What have you done...\nCheckout if it works...\n");
				}
				else
				{
					ret=-1;
					printf("Program exiting with failure\nHopefully not a big deal\n");
				}
				break;
		}
	}
    while( ret > -1);

	fclose(conffp);
	if(termbuf)
		free(termbuf);
	if(linebuf)
        free(linebuf);
	if(recbuf)
		free(recbuf);
	if(serrx)
		free(serrx);
	if(sertx)
		free(sertx);	
	exit(0);
}
