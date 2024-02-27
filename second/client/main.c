#define _GNU_SOURCE
#include <dlfcn.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

typedef struct dns_cache_entry {
    char hostname[256];
    struct hostent result;
    time_t timestamp;
} dns_cache_entry_t;

dns_cache_entry_t global_dns_cache = {0};

#define DNS_CACHE__BUFFER_SIZE 1024

static int (*original_gethostbyname_r)(const char *, struct hostent *, char *, size_t, struct hostent **, int *) = NULL;

int dns_cache__send_and_receive_unix_socket_message(const char *socket_path, const char *message, char *response) {
    int sock;
    struct sockaddr_un addr;
    ssize_t numBytes;
    struct timeval timeout;

    memset(response, 0, DNS_CACHE__BUFFER_SIZE);

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed\n");
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed\n");
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        close(sock);
        return -1;
    }

    if (write(sock, message, strlen(message)) == -1) {
        perror("write");
        close(sock);
        return -1;
    }

    numBytes = read(sock, response, DNS_CACHE__BUFFER_SIZE - 1);
    if (numBytes == -1) {
        perror("read");
        close(sock);
        return -1;
    }

    response[numBytes] = '\0';

    close(sock);
    return 0;
}

int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop) {
    if (!original_gethostbyname_r) {
        original_gethostbyname_r = dlsym(RTLD_NEXT, "gethostbyname_r");
    }
    const char *socket_path = "/dns_cache/dns.sock";
    char message[128];

    char response[DNS_CACHE__BUFFER_SIZE];

    snprintf(message, sizeof(message), "%s\n",name);

    if (dns_cache__send_and_receive_unix_socket_message(socket_path, message, response) == 0) {
        struct hostent h;
        char *h_name = strdup(name);
        char *h_aliases[] = { NULL };
        int h_addrtype = AF_INET;
        int h_length = sizeof(struct in_addr);
        struct in_addr addr;
        inet_aton(response, &addr);
        char *h_addr_list[] = { (char *)&addr, NULL };
        h.h_name = h_name;
        h.h_aliases = h_aliases;
        h.h_addrtype = h_addrtype;
        h.h_length = h_length;
        h.h_addr_list = h_addr_list;
        *result = &h;
        return 0;
    }
    printf("Fallback to original function\n");
    return original_gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
}
