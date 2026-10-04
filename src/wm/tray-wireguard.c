#include <sys/socket.h>

#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "string-util.h"
#include "tray-wireguard.h"

#define QUERY_INTERVAL_SECONDS 5
#define QUERY_TIMEOUT_SECONDS 3

enum {
	MAX_INTERFACE_NAMES = 128,
	REPLY_BATCH_LIMIT = 16,
	REPLY_BUFFER_SIZE = 32768
};

/*
 * Only link metadata is requested: no WireGuard keys or privileged queries.
 * Nonblocking dumps keep network discovery out of the WM's input path.
 */
static int fd = -1;
static unsigned int sequence;
static int pending;
static double started, next_query;
static char names[MAX_INTERFACE_NAMES][IFNAMSIZ];

static unsigned int count, extra;

static int	start_query(double);
static int	process_reply(const struct nlmsghdr *, char *, size_t);
static int	link_name(const struct nlmsghdr *, char *);
static int	compare_names(const void *, const void *);
static void	format_names(char *, size_t);
static void	failed(char *, size_t);

void
tray_wireguard_close(void)
{
	if (fd >= 0)
		close(fd);
	fd = -1;
	pending = 0;
}

static int
link_name(const struct nlmsghdr *header, char *name)
{
	const struct ifinfomsg *info = NULL;
	int remaining, wireguard;
	struct rtattr *attr = NULL;

	if (header->nlmsg_len < NLMSG_LENGTH(sizeof(struct ifinfomsg)))
		return 0;

	info = NLMSG_DATA(header);

	if ((info->ifi_flags & IFF_UP) == 0)
		return 0;

	wireguard = 0;

	name[0] = '\0';

	remaining = IFLA_PAYLOAD(header);

	for (attr = IFLA_RTA(info); RTA_OK(attr, remaining);
		attr = RTA_NEXT(attr, remaining)) {
		size_t size;
		int left, type;
		struct rtattr *part = NULL;

		type = attr->rta_type & NLA_TYPE_MASK;
		size = RTA_PAYLOAD(attr);

		if (type == IFLA_IFNAME && size > 1 && size <= IFNAMSIZ &&
			((char *)RTA_DATA(attr))[size - 1] == 0)
			memcpy(name, RTA_DATA(attr), size);

		if (type != IFLA_LINKINFO)
			continue;

		left = (int)size;

		for (part = RTA_DATA(attr); RTA_OK(part, left);
			part = RTA_NEXT(part, left))
			if ((part->rta_type & NLA_TYPE_MASK) ==
					IFLA_INFO_KIND &&
				RTA_PAYLOAD(part) == sizeof("wireguard") &&
				memcmp(RTA_DATA(part), "wireguard",
					sizeof("wireguard")) == 0)
				wireguard = 1;
	}

	return wireguard && *name != '\0';
}

static int
compare_names(const void *a, const void *b)
{
	return strcmp(a, b);
}

static void
format_names(char *text, size_t size)
{
	size_t used;
	unsigned int i;

	qsort(names, count, sizeof(names[0]), compare_names);

	if (count == 0 && extra == 0) {
		string_copy(text, "off", size);
		return;
	}

	text[0] = '\0';

	for (i = 0; i < count; i++) {
		size_t length;
		unsigned int left;
		char suffix[32];

		left = count - i - 1 + extra;
		snprintf(suffix, sizeof(suffix), left ? " +%u" : "", left);

		used = strlen(text);
		length = strlen(names[i]);

		if (used + (used != 0) + length + strlen(suffix) >= size) {
			snprintf(text + used, size - used, "%s+%u",
				used ? " " : "", count - i + extra);
			return;
		}

		snprintf(text + used, size - used, "%s%s", used ? " " : "",
			names[i]);
	}

	if (extra == 0)
		return;

	used = strlen(text);

	snprintf(text + used, size - used, " +%u", extra);
}

static void
failed(char *text, size_t size)
{
	tray_wireguard_close();
	string_copy(text, "?", size);
}

static int
start_query(double now)
{
	struct {
		struct nlmsghdr		header;
		struct ifinfomsg	info;
	} request = {0};
	struct sockaddr_nl kernel = {0};

	next_query = now + QUERY_INTERVAL_SECONDS;

	if (fd < 0)
		fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC,
			NETLINK_ROUTE);

	if (fd < 0)
		return 0;

	request.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	request.header.nlmsg_type = RTM_GETLINK;
	request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	request.header.nlmsg_seq = ++sequence;
	request.info.ifi_family = AF_UNSPEC;
	kernel.nl_family = AF_NETLINK;

	if (sendto(fd, &request, sizeof(request), 0, (struct sockaddr *)&kernel,
		sizeof(kernel)) < 0)
		return 0;

	count = extra = 0;
	pending = 1;
	started = now;

	return 1;
}

/*
 * Zero continues the dump, one completes it, and negative one reports failure.
 */
static int
process_reply(const struct nlmsghdr *header, char *text, size_t size)
{
	char name[IFNAMSIZ];

	if (header->nlmsg_seq != sequence)
		return 0;
	if (header->nlmsg_type == NLMSG_ERROR ||
		(header->nlmsg_flags & NLM_F_DUMP_INTR))
		return -1;
	if (header->nlmsg_type == NLMSG_DONE) {
		if (NLMSG_PAYLOAD(header, 0) >= sizeof(int)) {
			int error;

			memcpy(&error, NLMSG_DATA(header), sizeof(error));
			if (error)
				return -1;
		}
		pending = 0;
		format_names(text, size);
		return 1;
	}
	if (header->nlmsg_type != RTM_NEWLINK || !link_name(header, name))
		return 0;
	if (count >= sizeof(names) / sizeof(*names)) {
		extra++;
		return 0;
	}
	string_copy(names[count++], name, sizeof(names[0]));

	return 0;
}

void
tray_wireguard_update(char *text, size_t size, double now)
{
	int batch;

	if (size == 0)
		return;

	if (*text == '\0')
		string_copy(text, "...", size);

	if (!pending && now >= next_query && !start_query(now)) {
		failed(text, size);
		return;
	}

	if (!pending)
		return;

	/* Bound work per tick even if the host has many interfaces. */

	for (batch = 0; batch < REPLY_BATCH_LIMIT; batch++) {
		struct msghdr message = {0};
		struct iovec iov = {0};
		struct nlmsghdr *h = NULL;
		struct sockaddr_nl sender = {0};
		ssize_t length;
		int remaining;

		union {
			struct nlmsghdr	align;
			char		bytes[REPLY_BUFFER_SIZE];
		} buffer;

		iov.iov_base = buffer.bytes;
		iov.iov_len = sizeof(buffer.bytes);
		message.msg_name = &sender;
		message.msg_namelen = sizeof(sender);
		message.msg_iov = &iov;
		message.msg_iovlen = 1;
		length = recvmsg(fd, &message, 0);

		if (length < 0 && (errno == EAGAIN || errno == EINTR))
			break;

		if (length <= 0 || (message.msg_flags & MSG_TRUNC)) {
			failed(text, size);
			return;
		}

		if (sender.nl_pid)
			continue;

		remaining = (int)length;

		for (h = (void *)buffer.bytes; NLMSG_OK(h, remaining);
			h = NLMSG_NEXT(h, remaining)) {
			int result;

			result = process_reply(h, text, size);
			if (result < 0)
				failed(text, size);
			if (result != 0)
				return;
		}

		if (remaining) {
			failed(text, size);
			return;
		}
	}

	if (now - started >= QUERY_TIMEOUT_SECONDS)
		failed(text, size);
}
