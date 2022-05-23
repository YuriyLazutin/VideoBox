#ifndef DEFINES_H
#define DEFINES_H

#define   SERVER_PORT             5810
#define   SERVER_LISTEN_BACKLOG   32
#define   ID_SIZE                 4
#define   FLAGS_SIZE              4
#define   SIG_SIZE                32

#define   TINY_BUFFER_SIZE        8
#define   SMALL_BUFFER_SIZE       256
#define   STANDARD_BUFFER_SIZE    4096
#define   LARGE_BUFFER_SIZE       1048576

#define   NO_ERRORS              0
#define   BAD_REQUEST            1
#define   BAD_METHOD             2
#define   NOT_FOUND              3
#define   CONNECTION_CLOSED      4
#define   MALLOC_FILED           5
#define   PATH_OVERFLOW          6
#define   OPEN_FILE_ERROR        7
#define   READ_FILE_ERROR        8

#endif // DEFINES_H