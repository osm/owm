#include <assert.h>

#include "../src/shared/string-util.h"
#include "../src/wm/tray-wireguard.c"

static union {
	struct nlmsghdr	align;
	char		bytes[512];
} fixture;

static int		 show_live_status(void);
static void		 test_link_attributes(void);
static void		 test_interface_names(void);
static struct rtattr	*attribute(unsigned int, const void *, size_t);
static struct nlmsghdr	*link_fixture(const char *, int);

static struct rtattr *
attribute(unsigned int type, const void *data, size_t size)
{
	struct nlmsghdr *h = NULL;
	size_t offset;
	struct rtattr *a = NULL;

	h = (void *)fixture.bytes;
	offset = NLMSG_ALIGN(h->nlmsg_len);
	a = (void *)(fixture.bytes + offset);

	a->rta_type = type;
	a->rta_len = RTA_LENGTH(size);

	if (size)
		memcpy(RTA_DATA(a), data, size);
	h->nlmsg_len = offset + RTA_ALIGN(a->rta_len);

	return a;
}

static struct nlmsghdr *
link_fixture(const char *kind, int up)
{
	struct ifinfomsg *info = NULL;
	struct nlmsghdr *h = NULL;
	struct rtattr *nested = NULL;

	memset(&fixture, 0, sizeof(fixture));

	h = (void *)fixture.bytes;

	h->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	h->nlmsg_type = RTM_NEWLINK;

	info = NLMSG_DATA(h);

	info->ifi_flags = up ? IFF_UP : 0;
	attribute(IFLA_IFNAME, "office", sizeof("office"));

	nested = attribute(IFLA_LINKINFO | NLA_F_NESTED, NULL, 0);

	attribute(IFLA_INFO_KIND, kind, strlen(kind) + 1);
	nested->rta_len = fixture.bytes + h->nlmsg_len - (char *)nested;

	return h;
}

static int
show_live_status(void)
{
	int i;
	char text[64] = "";

	for (i = 0; i < 4; i++) {
		tray_wireguard_update(text, sizeof(text), i);

		if (strcmp(text, "..."))
			break;

		sleep(1);
	}

	puts(text);
	tray_wireguard_close();

	return strcmp(text, "?") == 0 || strcmp(text, "...") == 0;
}

static void
test_link_attributes(void)
{
	struct nlmsghdr *h = NULL;
	char name[IFNAMSIZ];

	assert(link_name(link_fixture("wireguard", 1), name));
	assert(!strcmp(name, "office"));
	assert(!link_name(link_fixture("wireguard", 0), name));
	assert(!link_name(link_fixture("bridge", 1), name));

	h = link_fixture("wireguard", 1);

	h->nlmsg_len--;
	assert(!link_name(h, name)); /* Truncated nested attribute. */
	h->nlmsg_len = NLMSG_HDRLEN;
	assert(!link_name(h, name));
}

static void
test_interface_names(void)
{
	char text[64] = "";

	format_names(text, sizeof(text));
	assert(!strcmp(text, "off"));
	string_copy(names[count++], "work", sizeof(names[0]));
	string_copy(names[count++], "home", sizeof(names[0]));
	string_copy(names[count++], "y", sizeof(names[0]));
	format_names(text, sizeof(text));
	assert(!strcmp(text, "home work y"));
	format_names(text, 9);
	assert(!strcmp(text, "home +2"));
	count = 1;
	string_copy(names[0], "y", sizeof(names[0]));
	format_names(text, sizeof(text));
	assert(!strcmp(text, "y")); /* Removed interfaces disappear. */
	extra = 130;
	format_names(text, sizeof(text));
	assert(!strcmp(text, "y +130"));
	count = extra = 0;
	format_names(text, sizeof(text));
	assert(!strcmp(text, "off"));
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && strcmp(argv[1], "--live") == 0)
		return show_live_status();

	test_link_attributes();
	test_interface_names();
	puts("PASS tray-wireguard");

	return 0;
}
