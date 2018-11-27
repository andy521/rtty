/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <uwsc/log.h>
#include <uwsc/buffer.h>
#include <ev.h>

#include "file.h"

static int state;

static int parse_file_data(struct buffer *b)
{
    static char name[1024] = "";
    static int fd;

    if (!state) {
        /* parse name */
        
        int name_len = ntohs(buffer_pull_u16(b));
        buffer_pull(b, name, name_len);

        fd = open(name, O_WRONLY | O_CREAT, 0644);

        state = 1;
        return parse_file_data(b);
    } else {
        const uint8_t *data = buffer_data(b);
        int len = buffer_length(b);

        if (len > 3 && data[len - 4] == 0x12 && data[len - 3] == 0x34 && data[len - 2] == 0x56 && data[len - 1] == 0x78) {
            len -= 4;
            if (len > 0)
                buffer_pull_to_fd(b, fd, len - 4, NULL, NULL);
            close(fd);
            state = 0;
            printf("save to %s\r\n", name);
            return 1;
        } else {
            buffer_pull_to_fd(b, fd, -1, NULL, NULL);
            return 0;
        }    
    }
}

static void read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    static struct buffer b;
    bool eof = false;

    buffer_put_fd(&b, w->fd, -1, &eof, NULL, NULL);

    if (parse_file_data(&b))
        ev_io_stop(loop, w);
}

void receive_file()
{
    struct ev_loop *loop = EV_DEFAULT;
    char head[] = {0x12, 0x34, 0x56, 0x78};
    struct termios otty, ntty;
    struct ev_io w;

    tcgetattr(STDIN_FILENO, &otty);

    ntty = otty;

    ntty.c_iflag = IGNBRK;
    /* No echo, crlf mapping, INTR, QUIT, delays, no erase/kill */
    ntty.c_lflag &= ~(ECHO | ICANON | ISIG);
    ntty.c_oflag = 0;	/* Transparent output */
    ntty.c_cc[VMIN] = 1;
    ntty.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &ntty);

    if (write(STDOUT_FILENO, head, 4) < 0) {
        uwsc_log_err("Write failed: %s\n", strerror(errno));
        exit(0);
    }

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);

    ev_io_init(&w, read_cb, STDIN_FILENO, EV_READ);
    ev_io_start(loop, &w);

    ev_run(loop, 0);

    tcsetattr(STDIN_FILENO, TCSADRAIN, &otty);

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);

    exit(0);
}

void send_file(const char *name)
{
    exit(0);
}

