
#ifndef _H_CMD_PROCESS
#define _H_CMD_PROCESS

#include<Arduino.h>
#include "printf.h"


class CommandBuffer {
private:
	char linebuf[512];
	size_t linebuf_len;
    char prev_line[512];
    bool skip_line;
	
public:
    Stream *src;
    char *cmds[64];
	size_t cmds_len;
    int r, g, b;

    CommandBuffer(Stream *src);
    CommandBuffer(Stream *src, int r, int g, int b);
    void read();
    char** parse(size_t *length);

    void print(const char *fmt, ...);
};


#endif
