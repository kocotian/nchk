#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "arg.h"
#include "util.c"

#define COMMAND(cmd, src) !strcmp((src), (cmd))
#define COMMAND_ARG(cmd, src) !strcmpt((src" "), (cmd), ' ')
#define GOTOXY(x, y) printf(yxstr, (y), (x))
#define GOUNDERT() GOTOXY(1, 10)

#define SUPERPOWERED(checker) (((checker) >> 7) & 1)
#define COLOR(checker) (((checker) >> 6) & 1)
#define COL(checker) (((checker >> 3) & 7) + 1)
#define ROW(checker) (((checker) & 7) + 1)

static void cmdmv(int16_t *checkers, char *cmd);
static void cmdreturn(int16_t *checkers);
static void drawchecker(int16_t checker);
static void dumpcheckers(int16_t *checkers);
static int16_t *getcheckerbypos(int16_t *checkers, int16_t row, int16_t col);
static void go(int16_t col, int16_t row);
static int16_t makechecker(int16_t superpowered, int16_t color, int16_t col, int16_t row);
static void prepare(int16_t *checkers);
static void usage(void);

static const char t[] =
"\033[1;97m   a  b  c  d  e  f  g  h\n"
"\033[1;97m1 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m2 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m3 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m4 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m5 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m6 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m7 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m8 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n";

char *argv0;

static const char yxstr[] = "\033[%d;%dH";
static int16_t color;

static void
cmdmv(int16_t *checkers, char *cmd)
{
	int16_t *checker, *dest;

	if (!strcmp(cmd, "help")) {
		usage:
		puts("usage: mv <src position> <dest position>");
		puts("   or: mv <src position> <movement>");
		puts("\nexample: mv c3 d4");
		puts(  "         mv c3 rd");
		puts("\n<movement> is 2 letters: [l]eft/[r]ight and [u]p/[d]own");
		return;
	}

	if (cmd[3] == 'l') cmd[3] = (cmd[0] - 1);
	else if (cmd[3] == 'r') cmd[3] = (cmd[0] + 1);

	if (cmd[4] == 'u' || cmd[4] == 't') cmd[4] = (cmd[1] - 1);
	else if (cmd[4] == 'd' || cmd[4] == 'b') cmd[4] = (cmd[1] + 1);

	if (
	!(   (cmd[0] >= 'a' && cmd[0] <= 'h')
	&&   (cmd[1] >= '1' && cmd[1] <= '8')
	&&   (cmd[2] == ' ')
	&&  ((cmd[3] >= 'a' && cmd[3] <= 'h') || (cmd[3] == 'l' || cmd[3] <= 'r'))
	&&  ((cmd[4] >= '1' && cmd[4] <= '8')
	||   (cmd[3] == 'u' || cmd[3] <= 'd' || cmd[3] == 't' || cmd[3] <= 'b'))
	)) goto usage;

	if ((checker = getcheckerbypos(checkers, cmd[0] - 'a' + 1, cmd[1] - '1' + 1)) == NULL) {
		puts("specified field is blank");
		return;
	}

	if (COLOR(*checker) != color) {
		puts("it's not your checker");
		return;
	}

	if ((dest = getcheckerbypos(checkers, cmd[3] - 'a' + 1, cmd[4] - '1' + 1)) != NULL) {
		if (COLOR(*dest) == color) {
			puts("you can't beat checker in your color");
			return;
		} else {
			*dest = -1;
			puts("*beating*");
		}
	}

	*checker = makechecker(
		SUPERPOWERED(*checker),
		COLOR(*checker),
		cmd[4] - '1',
		cmd[3] - 'a'
	);
}

static void
cmdreturn(int16_t *checkers)
{
}

static void
drawchecker(int16_t checker)
{
	if (checker < 0) return;
	go(COL(checker), ROW(checker));
	printf("\033[1;3%cm%c", COLOR(checker) + '1', SUPERPOWERED(checker) ? '@' : 'O');
}

static void
dumpcheckers(int16_t *checkers)
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j) {
			drawchecker(checkers[j + (i * 12)]);
		}
}

static int16_t *
getcheckerbypos(int16_t *checkers, int16_t row, int16_t col)
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j)
			if (ROW(checkers[j + (i * 12)]) == row && COL(checkers[j + (i * 12)]) == col)
				return &(checkers[j + (i * 12)]);
	return NULL;
}

static void
go(int16_t col, int16_t row)
{
	GOTOXY((3 * row) + 1, col + 1);
}

static int16_t
makechecker(int16_t superpowered, int16_t color, int16_t col, int16_t row)
{
	int16_t ret;
	ret = 0;
	ret += row & 7;
	ret += (col & 7) << 3;
	ret += (color & 1) << 6;
	ret += (superpowered & 1) << 7;
	return ret;
}

static void
prepare(int16_t *checkers)
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j) {
			checkers[j + (i * 12)] = 0;
			checkers[j + (i * 12)] += (i << 6);
			checkers[j + (i * 12)] += (((j / 4) + (i * 5)) << 3);
			checkers[j + (i * 12)] += (((j % 4) * 2) + (!(((j / 4) + i) % 2)));
		}
}

static void
usage(void)
{
	die("usage: %s [-h HOSTIP] [-p HOSTPORT] [CLIENT_IP [CLIENT_PORT]]", argv0);
}

static size_t
message(char *ip, uint16_t port, char *msg, size_t msgsiz, char **buf, size_t bufsiz)
{
	int sockfd;
	struct sockaddr_in addr;
	size_t rb = 0;

	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if ((sockfd = socket(addr.sin_family = AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return -1;

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return -1;

	if (send(sockfd, msg, msgsiz, 0) < 0)
		return -1;

	if (buf != NULL) {
		if ((rb = read(sockfd, *buf, bufsiz)) < 0)
			return -1;
	}

	close(sockfd);
	return rb;
}

static void
sendupdate(int16_t *checkers, char *ip, uint16_t port)
{
	char msg[39];
	msg[0] = 'U';
	memcpy(msg + 1, checkers, 38);
	if (message(ip, port, msg, 39, NULL, 0) < 0)
		die("message:");
}

static void
requestjoin(char *ip, uint16_t port, struct sockaddr_in addr)
{
	char msg[1 + sizeof(addr)] = "J";
	memcpy(msg + 1, &addr, sizeof(addr));
	if (message(ip, port, (char *)&msg, 1 + sizeof(addr), NULL, 0) < 0)
		die("message:");
}

int
main(int argc, char *argv[])
{
	char *line = NULL; size_t lnsiz = 0;
	uint16_t hport, cport; char *hip, *cip;
	pid_t forkpid, parentpid;
	int move;

	struct sockaddr_in haddr, caddr;

	/* 2 arrays of 12 checkers
	   in format:
	   [1 bit: superpowered][1 bit: color][3 bits: column][3 bits: row] */
	int16_t *checkers = mmap(NULL, sizeof(*checkers) * 24, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, 0, 0);

	hport = cport = 2321;
	hip   = cip   = "127.0.0.1";

	ARGBEGIN {
	case 'h':   hip = ARGF();						break;
	case 'p': hport = strtol(ARGF(), NULL, 10);		break;
	default: usage(); break;
	} ARGEND

	if (argc > 2)
		die("too many arguments (given: %d; required: from 0 to 2)", argc);

	move = 0;
	if (!argc) /* argc not given, hosting */
		color = 0;
	else /* argc given, joining */
		color = 1;

	if (argc > 0)
		cip = argv[0];
	if (argc > 1)
		cport = strtol(argv[1], NULL, 10);

	color = 1;
	prepare(checkers);

	parentpid = getpid();

	fork:
	{
		int sockfd, clientfd, opt;
		socklen_t caddrsiz;
		size_t resplen;

		char buffer[BUFSIZ];

		if ((sockfd = socket(haddr.sin_family = AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			die("socket:");

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					&opt, sizeof(opt)) < 0)
			die("setsockopt:");

		haddr.sin_port = htons(hport);
		haddr.sin_addr.s_addr = inet_addr(hip);
		caddrsiz = sizeof(caddr);

		if (bind(sockfd, (const struct sockaddr *)&haddr, sizeof(haddr)) < 0)
			die("bind:");
		if (listen(sockfd, 3) < 0)
			die("listen:");

		if ((forkpid = fork()) < 0)
			die("fork:");

		if (!forkpid) {
			while (1) {
				if ((clientfd = accept(sockfd, (struct sockaddr *)&caddr, &caddrsiz)) < 0)
					die("accept:");

				if ((resplen = read(clientfd, buffer, BUFSIZ)) < 0)
					die("read:");

				switch (*buffer) {
				case 'A': /* Accepted */
					kill(parentpid, SIGUSR1);
					break;
				case 'J': /* Join */ {
					char *l = malloc(BUFSIZ); size_t lsiz;
					char A = 'A'; struct sockaddr_in chost;
					memcpy(&chost, buffer + 1, resplen - 1);
					printf("accept connection request from %s:%d (hosts at %s:%d)? [y/n]: ",
							inet_ntoa(caddr.sin_addr), htons(caddr.sin_port),
							inet_ntoa(chost.sin_addr), htons(chost.sin_port));
					if (getline(&l, &lsiz, stdin) < 0)
						die("error while getting line:");
					if (*l == 'y' || *l == 'Y')
						kill(parentpid, SIGUSR1);
					message(inet_ntoa(chost.sin_addr), ntohs(chost.sin_port), &A, 1, NULL, 0);
					free(l);
					break;
				}
				case 'R': /* Return */
					break;
				case 'U': /* Update */
					memcpy(checkers, buffer + 1, resplen - 1);
					break;
				}

				close(clientfd);
			}
		}
	}

	{
		sigset_t sig; int signo;
		if (!argc)
			printf("\033[2J\033[Hhosting under %s:%d\nwaiting for other users...\n",
					hip, hport);
		else {
			printf("\033[2J\033[Hwaiting for %s:%d to acceptation...\n",
					cip, cport);
			requestjoin(cip, cport, haddr);
		}
		sigemptyset(&sig);
		sigaddset(&sig, SIGUSR1);
		sigprocmask(SIG_BLOCK, &sig, NULL);
		sigwait(&sig, &signo);
	}

	printf("\033[2J\033[H");

	while (1) {
		GOTOXY(1, 1);
		printf("%s", t);
		dumpcheckers(checkers);
		GOUNDERT();
		printf("\033[0;97mstatus: %s\033[0;97m\n", "\033[1;92myour move");
		printf("\033[0;97m$ ");
		if (getline(&line, &lnsiz, stdin) < 0)
			break;
		line[strlen(line) - 1] = '\0';
		printf("\033[2J\033[H");
		GOTOXY(1, 12);

		if (COMMAND_ARG(line, "mv")) {
			cmdmv(checkers, line + 3);
			sendupdate(checkers, cip, cport);
		} else if (COMMAND(line, "return"))
			cmdreturn(checkers);
		else if (COMMAND_ARG(line, "rm"))
			printf("removed '%s'\n", line + 3);
		else if (COMMAND(line, "quit") || COMMAND(line, "exit") || COMMAND(line, "bye"))
			break;
		else if (COMMAND(line, ""));
		else
			printf("%s: unknown command or bad syntax\n", line);
	}

	kill(forkpid, SIGTERM);
	munmap(checkers, sizeof(*checkers) * 24);
	printf("\033[2J\033[H");
	puts("goodbye!");
}
