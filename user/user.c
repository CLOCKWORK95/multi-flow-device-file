#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#define BLOCKING_OPS_MSG "This is a blocking operation: if the stream is locked you will wait (for max timeout millis).\n"
#define DATA "ciao a tutti\n"
#define SIZE strlen(DATA)
#define BYTES_READ 8
#define MINORS 128

char buff[4096];
char out[BYTES_READ];
int blocking_operations;



int configure(int fd) {
        int input = 0, ret;
        long int timeout;
        
        while (input != 7) {

                printf("\n\n\n\n");
                printf("************************************************ \n");
                printf("            Multi flow device file \n");
                printf("************************************************ \n");
                printf("                SESSION'S SETTINGS \n");
                printf("             --------------------- \n\n");
                printf("1. LOW PRIORITY DATA FLOW (ASYNCRONOUS WRITE OPS)\n2. HIGH PRIORITY DATA FLOW (SYNCRONOUS WRITE OPS)\n");
                printf("3. BLOCKING\n4. NON-BLOCKING\n5. BACK TO OPERATIONS\n");

                scanf( "%d", &input );
                input += 2;

                switch ( input ) {

                case 3: case 4: case 5: case 6:
                        if( input == 6 ) blocking_operations = 0;
                        ret = ioctl( fd, input, timeout );
                        if ( ret == -1 ) goto exit;
                        break;
                case 7:
                        break;
                default:
                        printf( "Warning: invalid input.\n" );
                }

                if ( input == 5 ) {
                        printf("Insert a timeout (millis) for blocking operations.\n");
                        scanf("%ld", &timeout);
                        while (timeout <= 0){
                                printf("Warning: timeout has to be a positive integer value (millis).\n Insert a valid value:\n");
                                scanf("%ld", &timeout);
                        }
                        
                        blocking_operations = 1;
                        ret = ioctl(fd, 7, timeout);
                        if (ret == -1) goto exit;
                }
        }

        return 0;

exit:

        printf("Error on ioctl() (%s)\n", strerror(errno));
        close(fd);
        return -1;

}



void *operations(int fd) {

        int input = 0, ret;

        while (input != 4) {

                printf("\n\n\n\n");
                printf("************************************************ \n");
                printf("            Multi flow device file \n");
                printf("************************************************ \n");
                printf("                SELECT AN OPCODE \n");
                printf("             --------------------- \n\n");

                printf( "1. WRITE TO STREAM\n2. READ FROM STREAM\n3. SESSION SETTINGS\n4. EXIT\n" );
                
                scanf( "%d", &input );

                switch (input) {

                        case 1:

                                if( blocking_operations ) printf( BLOCKING_OPS_MSG );
                                ret = write( fd, DATA, SIZE );
                                if ( ret == -1 ) printf( "Error on write operation (%s)\n", strerror(errno) );
                                else printf( "Written %d bytes of the input string : '%s'\n", ret, DATA );
                                
                                break;
                        case 2:
                                if( blocking_operations ) printf( BLOCKING_OPS_MSG );                
                                ret = read( fd, out, BYTES_READ );
                                if ( ret == -1 ) printf( "Error on read operation (%s)\n", strerror(errno) );
                                else printf( "Read %d Bytes : %s\n", ret, out );
                                memset( out, 0, BYTES_READ );
                                
                                break;
                        case 3:
                                if ( configure(fd) != 0 ) break;
                        case 4:
                                break;
                        default:
                                printf( "Warning: selected an invalid opcode.\n" );
                                
                }
                
        }

        close(fd);

        return NULL;
}



int main( int argc, char **argv ) {       
        
        int major, minor, i;
        
        char *path;
        
        blocking_operations = 0;
        
        if (argc < 4)
        {
                printf("Usage: sudo ./user [Path Device File] [Major Number] [Minor number]\n");
                return -1;
        }

        path = argv[1];
        major = strtol(argv[2], NULL, 10);
        minor = strtol(argv[3], NULL, 10);

        system("clear");
        printf("------------------------- \n");
        printf(" Multi flow device file \n");
        printf("------------------------- \n");   


        for (i = 0; i < MINORS; i++)
        {
                sprintf(buff, "mknod %s%d c %d %i", path, i, major, i);
                system(buff);

        }

        sprintf(buff, "%s%d", path, minor);

        printf("(NOTICE: 128 devices were created. Your is %d in [0;127])\n", minor);

        char *device = (char *)strdup(buff);

        int fd = open(device, O_RDWR);
        if (fd == -1)
        {
                printf("open error on device %s, %s\n", device, strerror(errno));
                return -1;
        }

        system("clear");

        if (configure(fd) != 0) return -1;
        
        operations(fd);

        return 0;
}