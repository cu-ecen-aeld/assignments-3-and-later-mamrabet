#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]){
	openlog(NULL,0,LOG_USER);
	if (argc != 3) {
		syslog(LOG_ERR, "Error: Two arguments required. \n Usage: ./writer <writefile> <writestr>");
		closelog();
		return 1;
    }
    const char *writefile = argv[1];
    const char *writestr = argv[2];
    
    FILE *f = fopen(writefile,"w");
    
    if (f==NULL){
    	syslog(LOG_ERR, "Error: File not found");
    	closelog();
	return 1;
    }
    
   fputs(writestr, f);
   syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
   closelog();
   return 0;
 }
