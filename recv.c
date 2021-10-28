/*
 * Copyright (c) 2021 Jan Klemkow <jan@openbsd.org>
 * Copyright (c) 2021 Moritz Buhl <mbuhl@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool run = true;

void
status(int sig)
{
	if (sig == SIGINT)
		run = false;
}

void
usage(void)
{
	fprintf(stderr, "recv [-m] file\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct timeval start, end;
	struct sockaddr_in saddr;
	ssize_t size;
	int ch, fd, s;
	bool mflag = false;
	size_t bytes = 0;

	while ((ch = getopt(argc, argv, "m")) != -1) {
		switch (ch) {
		case 'm':
			mflag = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	memset(&saddr, 0, sizeof saddr);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	saddr.sin_port = htons(1234);

	if ((fd = open(argv[0], O_WRONLY)) == -1)
		err(EXIT_FAILURE, "%s", argv[0]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(EXIT_FAILURE, "socket");

	if (bind(s, (struct sockaddr *)&saddr, sizeof saddr) == -1)
		err(EXIT_FAILURE, "bind");

	if (gettimeofday(&start, NULL) == -1)
		err(EXIT_FAILURE, "gettimeofday");

	if (signal(SIGINT, status) == SIG_ERR)
		err(EXIT_FAILURE, "signal");

	if (mflag) {
		struct mmsghdr	mmsg[256];
		struct iovec	iov[nitems(mmsg)];
		int cnt;

		for (size_t i = 0; i < nitems(mmsg); i++) {
			mmsg[i].msg_hdr.msg_iov = &iov[i];
			mmsg[i].msg_hdr.msg_iovlen = 1;

			iov[i].iov_base = malloc(BUFSIZ);
			iov[i].iov_len = BUFSIZ;
		}
 again:
		while (run && (cnt = recvmmsg(s, mmsg, nitems(mmsg), MSG_DONTWAIT, NULL)) > 0) {
			if ((size = writev(fd, iov, cnt)) == -1)
				err(EXIT_FAILURE, "writev");

			bytes += size;
		}

		if (cnt == -1) {
			if (errno == EAGAIN)
				goto again;

			err(EXIT_FAILURE, "recvmmsg");
		}
	} else {
		struct msghdr msg;
		char buf[BUFSIZ];

		memset(&msg, 0, sizeof msg);
		msg.msg_iov = &(struct iovec) {
			.iov_base = buf,
			.iov_len = sizeof buf,
		};
		msg.msg_iovlen = 1;

		while (run && (size = recvmsg(s, &msg, 0)) > 0) {
			if ((size = write(fd, msg.msg_iov->iov_base, size)) == -1)
				err(EXIT_FAILURE, "write");

			bytes += size;
		}

		if (size == -1)
			err(EXIT_FAILURE, "recvmsg");
	}

	if (close(s) == -1)
		err(EXIT_FAILURE, "close");
	if (close(fd) == -1)
		err(EXIT_FAILURE, "close");

	if (gettimeofday(&end, NULL) == -1)
		err(EXIT_FAILURE, "gettimeofday");

	printf("%zu bytes in %llu sec\n", bytes, end.tv_sec - start.tv_sec);
	printf("%llu bytes/s\n", bytes / (end.tv_sec - start.tv_sec));

	return EXIT_SUCCESS;
}
