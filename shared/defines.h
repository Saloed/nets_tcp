#pragma once
#define MESSAGE_SIZE 1024
#define SERVER_PORT 7777
#define MESSAGE_END "\r\n\r\n"
#define CMD_PREFIX "cmd:"
#define TXT_PREFIX "txt:"
#define JSON_PREFIX "jsn:"
#define ERROR_PREFIX "err:"
#define MESSAGE_PREFIX_LEN 4

#define ERROR_MESSAGE_SIZE (-2)
#define RECV_ERROR (-3)
#define RECV_TIMEOUT (-5)
