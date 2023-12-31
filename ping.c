#include "ping2.h"

struct proto proto_v4 = {proc_v4, send_v4, NULL, NULL, 0, IPPROTO_ICMP};

#ifdef IPV6
struct proto proto_v6 = {proc_v6, send_v6, NULL, NULL, 0, IPPROTO_ICMPV6};
#endif

int datalen = 56; /* data that goes with ICMP echo request */

int broadcast = 0; // 是否为广播信号
int ttl = 255;     // ttl值
int quiet = 0;     // 隐藏包信息
/*打印帮助信息*/
void printTips()
{
    printf("-h show help message\n");
    printf("-V show version\n");
    printf("-C show project repsitory\n");

    printf("----[options]----\n");
    printf("-v verbose\n");
    printf("-b allow broadcast ip\n");
    printf("-4 Ipv4 address only\n");
    printf("-6 Ipv6 address only\n");
    printf("-q quiet mode\n");
    printf("-d forbid dns resolve\n");
    printf("-D print latency\n");

    printf("----[parameters]----\n");
    printf("-t [ttl] set ttl\n");
    printf("-m [mtu] set mtu\n");
    printf("-n [num] send num packets before exit\n");
    printf("-s [icmp_seq] set icmp packet length\n");
    printf("-i [interval] set time interval between packet sent\n");
    printf("-z [icmp_seq] set icmp_seq\n");
    printf("-w [timeout] set timeout between packet received\n");
    printf("-F [Flowlabel] set flowlabel\n");
    printf("-I [interface] set interface\n");
    printf("-O [filepath] redirect output to file\n");

    exit(0);
}
int main(int argc, char **argv)
{
    int j4, j6 = 0;
    int c;
    struct addrinfo *ai;

    opterr = 0; /* don't want getopt() writing to stderr */
    while ((c = getopt(argc, argv, "qvbht:46")) != -1)
    { // 命令行参数
        switch (c)
        {
        case 'v':
            verbose++; // 详细模式，即不是回复信息也打印
            break;
        case 'b':        // ping一个广播地址
            broadcast++; // 进行广播
            break;
        case 'h': // 帮助信息打印
            printTips();
            break;
        case 't':                             // 设置ttl
            sscanf(argv[optind], "%d", &ttl); // 读取数值
            if (ttl < 1)
            {
                printf("ttl must more than 0");
                exit(0);
            }
            break;
        case 'q': // 安静模式，不显示回复信息
            quiet = 1;
            break;
        case '?':
            err_quit("unrecognized option: %c", c);

        case '4': // 判断是否是ipv4协议
            j4++;
            break;

        // check ipv6 address
        case '6': // 判断是否是ipv6协议 ping -v -6 ip
            j6++;
            break;
        }
    }

    if (optind != argc - 1)
        err_quit("usage: ping [ -v ] <hostname>");
    host = argv[optind]; // 要ping的主机名

    if (j4)
    {
        Check_IPV4(host);
    }

    if (j6)
    {
        Check_IPV6(host);
    }

    pid = getpid();
    signal(SIGALRM, sig_alrm); // 信息传递，收到SIGALARM信号就执行sig_alrm

    ai = host_serv(host, NULL, 0, 0); // host_serv :使用给定的主机名和服务名，以及其他参数，获取对应的地址信息: 主机名，服务器名，IPV4 OR IPV6 ,TCP OR UDP

    printf("ping %s (%s): %d data bytes\n", ai->ai_canonname,
           Sock_ntop_host(ai->ai_addr, ai->ai_addrlen), datalen); // 打印形如 “ping <规范名称> (<主机名或IP地址>): <数据字节数> data bytes” 的ping信息

    /* 4initialize according to protocol */

    if (ai->ai_family == AF_INET)
    { // 如果是 ipv4 使用 proto_v4协议
        pr = &proto_v4;
#ifdef IPV6
    }
    else if (ai->ai_family == AF_INET6)
    { // 如果是 ipv4 使用 proto_v6协议
        pr = &proto_v6;
        if (IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6 *)
                                        ai->ai_addr)
                                       ->sin6_addr)))
            err_quit("cannot ping IPv4-mapped IPv6 address");
#endif
    }
    else
        err_quit("unknown address family %d", ai->ai_family);

    pr->sasend = ai->ai_addr;               // 目的地址
    pr->sarecv = calloc(1, ai->ai_addrlen); // 地址长度 ？
    pr->salen = ai->ai_addrlen;             // 地址长度？

    readloop();

    exit(0);
}
void Check_IPV4(char *input)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, input, &(sa.sin_addr));
    if (result != 1)
    {
        printf("%s is not a valid IPv4 address.\n", input);//不符合ipv4协议输出错误
    }
    return;
}
void Check_IPV6(char *input)
{
    struct in6_addr addr;
    if (inet_pton(AF_INET6, input, &addr) != 1)
    {
        fprintf(stderr, "%s is not a valid IPv6 address.\n", input);
        exit(EXIT_FAILURE);
    }
}

void proc_v4(char *ptr, ssize_t len, struct timeval *tvrecv)
{
    int hlen1, icmplen;
    double rtt;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;

    ip = (struct ip *)ptr; /* start of IP header */   // IP头
    hlen1 = ip->ip_hl << 2; /* length of IP header */ // IP头长度，因为ip_hl是每四个字节为一个长度

    icmp = (struct icmp *)(ptr + hlen1); /* start of ICMP header */
    if ((icmplen = len - hlen1) < 8)
        err_quit("icmplen (%d) < 8", icmplen);

    if (icmp->icmp_type == ICMP_ECHOREPLY && !quiet)
    { // 如果是回复
        if (icmp->icmp_id != pid)
            return; /* not a response to our ECHO_REQUEST */ // 如果不是当前进程的回复
        if (icmplen < 16)
            err_quit("icmplen (%d) < 16", icmplen);

        tvsend = (struct timeval *)icmp->icmp_data;               // 数据为发送时间
        tv_sub(tvrecv, tvsend);                                   // 接收时间减去发送时间
        rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0; // 计算往返时间

        printf("%d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n", // 打印信息
               icmplen, Sock_ntop_host(pr->sarecv, pr->salen),
               icmp->icmp_seq, ip->ip_ttl, rtt);
    }
    else if (verbose && !quiet)
    { // 收到的不是回复但是是详细模式，要显示
        printf("  %d bytes from %s: type = %d, code = %d\n",
               icmplen, Sock_ntop_host(pr->sarecv, pr->salen),
               icmp->icmp_type, icmp->icmp_code);
    }
}

void proc_v6(char *ptr, ssize_t len, struct timeval *tvrecv)
{
#ifdef IPV6
    int hlen1, icmp6len;
    double rtt;
    struct ip6_hdr *ip6;
    struct icmp6_hdr *icmp6;
    struct timeval *tvsend;

    /*
    ip6 = (struct ip6_hdr *) ptr;		// start of IPv6 header
    hlen1 = sizeof(struct ip6_hdr);
    if (ip6->ip6_nxt != IPPROTO_ICMPV6)
        err_quit("next header not IPPROTO_ICMPV6");

    icmp6 = (struct icmp6_hdr *) (ptr + hlen1);
    if ( (icmp6len = len - hlen1) < 8)
        err_quit("icmp6len (%d) < 8", icmp6len);
    */

    icmp6 = (struct icmp6_hdr *)ptr;
    if ((icmp6len = len) < 8) // len-40
        err_quit("icmp6len (%d) < 8", icmp6len);

    if (icmp6->icmp6_type == ICMP6_ECHO_REPLY)
    {
        if (icmp6->icmp6_id != pid)
            return; /* not a response to our ECHO_REQUEST */
        if (icmp6len < 16)
            err_quit("icmp6len (%d) < 16", icmp6len);

        tvsend = (struct timeval *)(icmp6 + 1);
        tv_sub(tvrecv, tvsend);
        rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

        printf("%d bytes from %s: seq=%u, hlim=%d, rtt=%.3f ms\n",
               icmp6len, Sock_ntop_host(pr->sarecv, pr->salen),
               icmp6->icmp6_seq, ip6->ip6_hlim, rtt);
    }
    else if (verbose)
    {
        printf("  %d bytes from %s: type = %d, code = %d\n",
               icmp6len, Sock_ntop_host(pr->sarecv, pr->salen),
               icmp6->icmp6_type, icmp6->icmp6_code);
    }
#endif /* IPV6 */
}

unsigned short
in_cksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    /* 4mop up an odd byte, if necessary */
    if (nleft == 1)
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    /* 4add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                 /* add carry */
    answer = ~sum;                      /* truncate to 16 bits */
    return (answer);
}

void send_v4(void)
{
    int len;
    struct icmp *icmp;

    icmp = (struct icmp *)sendbuf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = pid;
    icmp->icmp_seq = nsent++;
    gettimeofday((struct timeval *)icmp->icmp_data, NULL);

    len = 8 + datalen; /* checksum ICMP header and data */
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = in_cksum((u_short *)icmp, len);

    sendto(sockfd, sendbuf, len, 0, pr->sasend, pr->salen);
}

void send_v6()
{
#ifdef IPV6
    int len;
    struct icmp6_hdr *icmp6;

    icmp6 = (struct icmp6_hdr *)sendbuf;
    icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id = pid;
    icmp6->icmp6_seq = nsent++;
    gettimeofday((struct timeval *)(icmp6 + 1), NULL);

    len = 8 + datalen; /* 8-byte ICMPv6 header */

    sendto(sockfd, sendbuf, len, 0, pr->sasend, pr->salen);
    /* kernel calculates and stores checksum for us */
#endif /* IPV6 */
}

void readloop(void)
{
    int size;
    char recvbuf[BUFSIZE];
    socklen_t len;
    ssize_t n;
    struct timeval tval;

    sockfd = socket(pr->sasend->sa_family, SOCK_RAW, pr->icmpproto); // 获取套接字。

    setuid(getuid()); /* don't need special permissions any more */

    size = 60 * 1024; /* OK if setsockopt fails */
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (broadcast == 1)
    { // 设置广播

        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
        {

            err_sys("set broadcast error");
            close(sockfd);
            exit(1);
        }
    }
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == -1)
    {
        err_sys("set broadcast error"); // 设置ttl
        exit(1);
    }
    sig_alrm(SIGALRM); /* send first packet */ /// 上面的是初始化，然后发送？

    for (;;)
    {
        len = pr->salen;
        n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, pr->sarecv, &len); // 接收回复
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            else
                err_sys("recvfrom error");
        }

        gettimeofday(&tval, NULL);       // 得到当前时间
        (*pr->fproc)(recvbuf, n, &tval); // 交给fproc处理
    }
}

void sig_alrm(int signo)
{
    (*pr->fsend)();

    alarm(1); // 设置一秒的计时器，计时结束产生信号重新执行
    return;   /* probably interrupts recvfrom() */
}

void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    { /* out -= in */
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}

char *
sock_ntop_host(const struct sockaddr *sa, socklen_t salen)
{
    static char str[128]; /* Unix domain is largest */

    switch (sa->sa_family)
    {
    case AF_INET:
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;

        if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
            return (NULL);
        return (str);
    }

#ifdef IPV6
    case AF_INET6:
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

        if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
            return (NULL);
        return (str);
    }
#endif

#ifdef HAVE_SOCKADDR_DL_STRUCT
    case AF_LINK:
    {
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

        if (sdl->sdl_nlen > 0)
            snprintf(str, sizeof(str), "%*s",
                     sdl->sdl_nlen, &sdl->sdl_data[0]);
        else
            snprintf(str, sizeof(str), "AF_LINK, index=%d", sdl->sdl_index);
        return (str);
    }
#endif
    default:
        snprintf(str, sizeof(str), "sock_ntop_host: unknown AF_xxx: %d, len %d",
                 sa->sa_family, salen);
        return (str);
    }
    return (NULL);
}

char *
Sock_ntop_host(const struct sockaddr *sa, socklen_t salen)
{
    char *ptr;

    if ((ptr = sock_ntop_host(sa, salen)) == NULL)
        err_sys("sock_ntop_host error"); /* inet_ntop() sets errno */
    return (ptr);
}

struct addrinfo *
host_serv(const char *host, const char *serv, int family, int socktype)
{
    int n;
    struct addrinfo hints, *res;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME; /* always return canonical name */
    hints.ai_family = family;      /* AF_UNSPEC, AF_INET, AF_INET6, etc. */
    hints.ai_socktype = socktype;  /* 0, SOCK_STREAM, SOCK_DGRAM, etc. */

    if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
        return (NULL);

    return (res); /* return pointer to first on linked list */
}
/* end host_serv */

static void
err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
    int errno_save, n;
    char buf[MAXLINE];

    errno_save = errno; /* value caller might want printed */
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf, sizeof(buf), fmt, ap); /* this is safe */
#else
    vsprintf(buf, fmt, ap); /* this is not safe */
#endif
    n = strlen(buf);
    if (errnoflag)
        snprintf(buf + n, sizeof(buf) - n, ": %s", strerror(errno_save));
    strcat(buf, "\n");

    if (daemon_proc)
    {
        syslog(level, buf);
    }
    else
    {
        fflush(stdout); /* in case stdout and stderr are the same */
        fputs(buf, stderr);
        fflush(stderr);
    }
    return;
}

/* Fatal error unrelated to a system call.
 * Print a message and terminate. */

void err_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(0, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(1);
}

/* Fatal error related to a system call.
 * Print a message and terminate. */

void err_sys(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(1, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(1);
}
