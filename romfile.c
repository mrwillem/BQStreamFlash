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
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include "romfile.h"

#define uchar unsigned char
/*
* A function to get hex bytes from ascii
*/
int atoh(uint8_t * recbuf,char * buf, int len)
{
	int i;
	int k;
	uint8_t checksum;
	k=0;
	for(i=1; i<len; i++)
	{
		/*
		 * if value smaller 0x3A it is a digit
		 */
		if(buf[i]> 0x29 && buf[i] < 0x3A)
		{
			recbuf[k]=(uint8_t)((buf[i]-0x30)<<4);
		}

		/*
		 * if value smaller 0x47 should be an character
		 */
		else if(buf[i]>0x40 && buf[i]< 0x47)
		{
			recbuf[k]=(uint8_t)((buf[i]-0x37)<<4);
		}
		else
		/*
	 	*  Data not valid
	 	*/
		{
			continue;
		}
		/*
	 	* decode the lower nibble
	 	*/
		if(buf[i+1]> 0x29 && buf[i+1] < 0x3A)
		{
			recbuf[k]+=(uint8_t)(buf[i+1]-0x30);
			k++;
			i++;
		}
		else if(buf[i+1]>0x40 && buf[i+1]< 0x47)
		{
			recbuf[k]+=(uint8_t)(buf[i+1]-0x37);
			k++;
			i++;
		}
		else
		{
			recbuf[k]=(uint8_t)0;
		}
	}
	printf("Decoded %i Bytes\n", k);
	recbuf[k]=0x0;
	return k;
}

/*
' A function to decode one line of the input file and output it
*/
int decodeline(FILE * conffp, char * buf, size_t *len, uint8_t *recbuf)
{

	ssize_t read;
	char c;
	int i, k;
	i=0;
	read = getline(&buf, len, conffp);
	if( read != -1)
	{
		printf("Retreived line of length %zu ;\n", read);
		printf("%s", buf);
	}
	else
	{
		return -1;
	}
	/*
	* Detect comments
	*/
	switch(buf[0])
	{
	case ';':
		return 1;
		break;
	case 'W':
		recbuf[128]=atoh(recbuf, buf, read);
		return 2;
		break;
	case 'C':
		recbuf[128]=atoh(recbuf, buf, read);
		return 3;
		break;
	case 'X':
		i=2;
		k=1;
		do
		{
			c=buf[i];
			i++;
			if(c > 0x29 && c < 0x41)
			{
				recbuf[k]=(uint8_t) (c-0x30);
				k++;
			}
		}
		while( ((c > 0x29 && c < 0x41) || c==' ') && i<read);
		/*
		* Store number of received chars in recbuf [0]
		*/
		recbuf[0]=(uint8_t) k-1;
		return 4;
		break;
	default:
		printf("nodata\n");
		break;
	}
	return 0;
}
