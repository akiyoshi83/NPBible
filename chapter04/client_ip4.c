#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/* サーバーにソケット接続 */
int
client_socket(const char *hostnm, const char *portnm)
{
    char buf[MAXHOSTNAMELEN];
    struct sockaddr_in server;
    struct in_addr addr;
    int soc, portno;
    struct hostent *host;
    struct servent *se;

    /* アドレス情報のヒントをゼロクリア */
    (void) memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    /* ホスト名がIPadoresuto仮定してホスト情報取得 */
    if (inet_pton(AF_INET, hostnm, &addr) == 0) {
        /* ホスト名が名称としてホスト情報取得 */
        if ((host = gethostbyname2(hostnm, AF_INET)) == NULL) {
            /* ホストが見つからない */
            (void) fprintf(stderr, "gethostbyname2():error\n");
            return (-1);
        }
        (void) memcpy(&addr, (struct in_addr *) *host->h_addr_list, sizeof(struct in_addr));
    }
    (void) fprintf(stderr, "addr=%s\n", inet_ntop(AF_INET, &addr, buf, sizeof(buf)));
    /* ホストアドレスのセット */
    server.sin_addr = addr;
    /* ポート番号の決定 */
    if (isdigit(portnm[0])) {
        /* 先頭が数字 */
        if ((portno = atoi(portnm)) <= 0) {
            /* 数値化するとゼロ以下 */
            (void) fprintf(stderr, "bad portno\n");
            return (-1);
        }
        server.sin_port = htons(portno);
    } else {
        if ((se = getservbyname(portnm, "tcp")) == NULL) {
            /* サービスが見つからない */
            (void) fprintf(stderr, "getservbyname():error\n");
            return (-1);
        } else {
            /* サービスが見つかった:該当ポート番号 */
            server.sin_port = se->s_port;
        }
    }
    (void) fprintf(stderr, "port=%d\n", ntohs(server.sin_port));
    /* ソケットの生成 */
    // ソケットを作成するが通常クライアントでは
    // IPアドレスやポート番号を固定する必要がないためbindはしない
    // Firewallを通したいなどの理由でポートを固定したい場合は別
    // IPアドレスを固定したい場合もあるかもしれないがあまりケースが思いつかない
    if ((soc = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return (-1);
    }
    /* コネクト */
    if (connect(soc, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("connect");
        (void) close(soc);
        return (-1);
    }
    return (soc);
}

/* 送受信処理 */
void
send_recv_loop(int soc)
{
    char buf[512];
    struct timeval timeout;
    int end, width;
    ssize_t len;
    fd_set mask, ready;

    /* select()用マスク */
    FD_ZERO(&mask);
    /* ソケットディスクリプタをセット */
    FD_SET(soc, &mask);
    /* 標準入力をセット */
    FD_SET(0, &mask);
    width = soc + 1;
    /* 送受信 */
    for (end = 0;;) {
        /* マスクの代入 */
        // select()内部で変更されるので毎回コピー
        ready = mask;
        /* タイムアウト値のセット */
        // select()内部で変更されるので毎回コピー
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        switch(select(width, (fd_set *) &ready, NULL, NULL, &timeout)) {
        case -1:
            /* エラー */
            perror("select");
            break;
        case 0:
            /* タイムアウト */
            break;
        default:
            /* レディ有り */
            /* ソケットレディ */
            if (FD_ISSET(soc, &ready)) {
                /* 受信 */
                if ((len = recv(soc, buf, sizeof(buf), 0)) == -1) {
                    /* エラー */
                    perror("recv");
                    end = 1;
                    break;
                }
                if (len == 0) {
                    /* EOF */
                    (void) fprintf(stderr, "recv:EOF\n");
                    end = 1;
                    break;
                }
                /* 文字列化・表示 */
                buf[len] = '\0';
                (void) fprintf(stderr, "> %s", buf);
            }
            /* 標準入力レディ */
            if (FD_ISSET(0, &ready)) {
                /* 標準入力から1行読み込み */
                (void) fgets(buf, sizeof(buf), stdin);
                if (feof(stdin)) {
                    end = 1;
                    break;
                }
                /* 送信 */
                if ((len = send(soc, buf, strlen(buf), 0)) == -1) {
                    /* エラー */
                    perror("send");
                    end = 1;
                    break;
                }
            }
            break;
        }
        if (end) {
            break;
        }
    }

}

int
main(int argc, char *argv[])
{
    int soc;
    /* 引数にホスト名、ポート番号が指定されているか？ */
    if (argc <= 2) {
        (void) fprintf(stderr, "client server-host port\n");
        return (EX_USAGE);
    }
    /* サーバーにソケット接続 */
    if ((soc = client_socket(argv[1], argv[2])) == -1) {
        (void) fprintf(stderr, "client_socket():err\n");
        return (EX_UNAVAILABLE);
    }
    /* 送受信処理 */
    send_recv_loop(soc);
    /* ソケットクローズ */
    (void) close(soc);
    return (EX_OK);
}

