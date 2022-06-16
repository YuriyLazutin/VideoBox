#ifndef DEFINES_H
#define DEFINES_H

#define   SERVER_PORT                     5810
#define   SERVER_LISTEN_BACKLOG           32
#define   ID_SIZE                         4
#define   FLAGS_SIZE                      4
#define   SIG_SIZE                        32
#define   TOTAL_REQ_TIME_LIMIT            30000 // 30 sec
#define   READ_BLOCK_TIME_LIMIT           5000 // 5 sec
#define   WRITE_BLOCK_TIME_LIMIT          5000 // 5 sec
#define   HEAD_LINE_LIMIT                 100
#define   MAX_CLOSE_DESCRIPTOR_ATTEMPTS   5
#define   MAX_CREATE_ID_DIR_ATTEMPTS      1000
#define   SIG_CHARS                       "0123456789abcdef"
#define   ID_CHARS                        "0123456789abcdefghijklmnopqrstuvwxy"

#define   TINY_BUFFER_SIZE        8
#define   SMALL_BUFFER_SIZE       256
#define   STANDARD_BUFFER_SIZE    4096
#define   LARGE_BUFFER_SIZE       1048576

#define   NO_ERRORS              0
#define   BAD_REQUEST            1
#define   BAD_METHOD             2
#define   NOT_FOUND              3
#define   CONNECTION_CLOSED      4
#define   MALLOC_FAILED          5
#define   PATH_OVERFLOW          6
#define   PATH_INVALID           7
#define   OPEN_FILE_ERROR        8
#define   READ_FILE_ERROR        9
#define   READ_LINK_ERROR       10
#define   TIME_OUT              11
#define   POLL_ERROR            12
#define   READ_BLOCK_ERROR      13
#define   WRITE_BLOCK_ERROR     14
#define   OPEN_DIR_ERROR        15
#define   STAT_FAILED           16
#define   PIPE_FAILED           17
#define   FORK_FAILED           18
#define   DUP2_FAILED           19
#define   EXEC_FAILED           20
#define   WAITPID_FAILED        21
#define   DUPLICATE_FOUND       22
#define   NOT_FILE              23
#define   RENAME_FILE_FAILED    24
#define   LIMIT_EXCEEDED        25
#define   LINE_LIMIT_EXCEEDED   26
#define   BUFFER_OVERFLOW       27
#define   INVALID_RANGE         28
#define   UNKNOWN_ERROR       1000

#endif // DEFINES_H
