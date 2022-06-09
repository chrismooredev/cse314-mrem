
#include "cmd_process.h"

CommandBuffer::CommandBuffer(Stream *src) : CommandBuffer(src, 0, 0, 0) {}
CommandBuffer::CommandBuffer(Stream *src, int r, int g, int b) {
    this->src = src;
    this->skip_line = false;
	this->linebuf_len = 0;
	this->cmds_len = 0;
    this->r = r;
    this->g = g;
    this->b = b;
}

void CommandBuffer::read() {
    while(this->src->available()) {
        char c = this->src->read();
        if(c == '\r') continue;

        // this->printf_("linebuf_len = %d   this->skip_line=%d   next character: %02hhx\n", this->linebuf_len, this->skip_line, c);

        // if this isn't a skipped line, and we only have room for newline, and this isn't newline
        if(!this->skip_line && this->linebuf_len >= sizeof(this->linebuf)-2 && c != '\n') {
            // skip this line
            this->skip_line = true;
            continue;
        }
        
        // if this line is being skipped, and this is the terminating newline char, reset buf
        if(this->skip_line && c == '\n') {
            this->linebuf_len = 0;
            continue;
        }

        // string functions prefer null terminators
        if(c == '\n') c = '\0';

        // otherwise put this in the buffer
        this->linebuf[this->linebuf_len] = c;
        ++this->linebuf_len;
    }
}

char** CommandBuffer::parse(size_t *length) {
    if(this->linebuf_len > 0 && this->linebuf[this->linebuf_len-1] == '\0') {
        size_t lbl = this->linebuf_len;
        size_t slen = strlen(this->linebuf);
        // this->printf_("parsed with linebuf len = %d, strlen = %d (last char = %02hhx)\n", lbl, slen, this->linebuf[this->linebuf_len-2]);

        memcpy(this->prev_line, this->linebuf, this->linebuf_len);
        this->linebuf_len = 0;

        this->cmds_len = 0;
		char *s = strtok(this->prev_line, ",");
		while (s != NULL && this->cmds_len < (sizeof(this->cmds)/sizeof(this->cmds[0]) - 1)) {
            this->cmds[this->cmds_len] = s;
            ++this->cmds_len;
			s = strtok(NULL, ",");
		}
        this->cmds[this->cmds_len] = nullptr;
        if(length != nullptr) { *length = this->cmds_len; }
        return this->cmds;
    }

    return nullptr;
}

// Printf. Can print messages up to 1K in size. No floats or doubles.
void CommandBuffer::print(const char *fmt, ...) {
    // hopefully messages won't reach 1K
    static char buf[1024];

    // if(! this->src->availableForWrite()) return;

    // forward the variable arguments over to vsnprintf
    va_list argp;
    va_start(argp, fmt);
    int written = vsnprintf_(buf, sizeof(buf), fmt, argp);
    va_end(argp);

    if(written < 0 || written >= sizeof(buf)) {
        this->src->println("Attempted to write formatted message that exceeds 1K printf buffer size:");
        size_t fmt_size = strlen(fmt);
        this->src->write(fmt, fmt_size);
        if(fmt[fmt_size-1] != '\n') {
            this->src->write('\n');
        }
    } else {
        this->src->print(buf);
    }

    // static char fmtbuf[1024];
    // static char buf[1024];

    // strcpy(fmtbuf, fmt);
    // size_t fmt_len = strlen(fmtbuf);

    // // find a float
    // for(size_t i = 0; i < fmt_len; ++i) {
        
    // }
}
