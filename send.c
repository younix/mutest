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

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
usage(void)
{
	fprintf(stderr, "send [-m] file\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in saddr;
	ssize_t size;
	int ch, fd, s;
	bool mflag = false;

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

	if ((fd = open(argv[0], O_RDONLY)) == -1)
		err(EXIT_FAILURE, "%s", argv[0]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(EXIT_FAILURE, "socket");

	if (mflag) {
		struct mmsghdr	mmsg[256];
		struct iovec	iov[nitems(mmsg)];

		for (size_t i = 0; i < nitems(mmsg); i++) {
			mmsg[i].msg_hdr.msg_name = &saddr;
			mmsg[i].msg_hdr.msg_namelen = sizeof saddr;
			mmsg[i].msg_hdr.msg_iov = &iov[i];
			mmsg[i].msg_hdr.msg_iovlen = 1;

			iov[i].iov_base = malloc(BUFSIZ);
			iov[i].iov_len = BUFSIZ;
		}

		while ((size = readv(fd, iov, nitems(iov))) > 0) {
			unsigned int vlen = size / BUFSIZ;
			unsigned int cnt = 0;

			if (vlen * BUFSIZ < size)
				vlen++;

			do {
				int ret = sendmmsg(s, mmsg+cnt, vlen-cnt, 0);
				if (ret == -1)
					err(EXIT_FAILURE, "sendmsg");
				cnt += ret;
			} while (cnt < vlen);
		}


	} else {
		struct msghdr msg;
		char buf[BUFSIZ];

		memset(&msg, 0, sizeof msg);
		msg.msg_name = &saddr;
		msg.msg_namelen = sizeof saddr;
		msg.msg_iov = &(struct iovec) {
			.iov_base = buf,
		};
		msg.msg_iovlen = 1;

		while ((size = read(fd, buf, sizeof buf)) > 0) {
			msg.msg_iov->iov_len = size;
			if (sendmsg(s, &msg, 0) == -1)
				err(EXIT_FAILURE, "sendmsg");
		}

		if (size == -1)
			err(EXIT_FAILURE, "read");
	}

	if (close(s) == -1)
		err(EXIT_FAILURE, "close");
	if (close(fd) == -1)
		err(EXIT_FAILURE, "close");

	return EXIT_SUCCESS;
}
