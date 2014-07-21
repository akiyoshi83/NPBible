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

void send_recv_loop(int acc);

/* サーバーソケットの準備 */
// getaddrinfo()を使用しないバージョン(IPv6)
// 複雑になるだけなのでgetaddrinfo()が使えない場合以外はやらない方が良い
int
server_socket(const char *portnm)
{
    struct sockaddr_in6 my;
    int soc, portno, opt;
    socklen_t opt_len;
    struct servent *se;

    /* アドレス情報のヒントをゼロクリア */
    (void) memset(&my, 0, sizeof(my));
    my.sin6_family = AF_INET6;
    // ネットワークバイトオーダーに変換
    my.sin6_addr = in6addr_any;
    /* ポート番号の決定 */
    if (isdigit(portnm[0])) {
        /* 先頭が数字 */
        if ((portno = atoi(portnm)) <= 0) {
            /* 数値化するとゼロ以下 */
            (void) fprintf(stderr, "bad port no\n");
            return (-1);
        }
        // ネットワークバイトオーダーに変換
        my.sin6_port = htons(portno);
    } else {
        if ((se = getservbyname(portnm, "tcp")) == NULL) {
            /* サービスが見つからない */
            (void) fprintf(stderr, "getservbyname():error\n");
            return (-1);
        } else {
            /* サービスが見つかった:該当ポート番号 */
            my.sin6_port = se->s_port;
        }
    }
    (void) fprintf(stderr, "port=%d\n", ntohs(my.sin6_port));

    /* ソケットの生成 */
    if ((soc = socket(PF_INET6, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return (-1);
    }

    /* ソケットオプション（再利用フラグ）設定 */
    opt = 1;
    opt_len = sizeof(opt);
    if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len)) {
        perror("setsockopt");
        (void) close(soc);
        return (-1);
    }

    /* ソケットにアドレスを指定 */
    if (bind(soc, (struct sockaddr *) &my, sizeof(my)) == -1) {
        perror("bind");
        (void) close(soc);
        return (-1);
    }

    /* アクセスバックログの指定 */
    // listenすると接続待ち受け可能な状態のsocketになる
    // アクセスバックログとは接続待ちのキューの数のことらしい
    if (listen(soc, SOMAXCONN) == -1) {
        perror("listen");
        (void) close(soc);
        return (-1);
    }
    // ここで解放して良いらしい
    return (soc);
}

/* アクセプトループ */
void
accept_loop(int soc)
{
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    struct sockaddr_storage from;
    int acc;
    socklen_t len;
    // 繰り返し待ち受ける
    for (;;) {
        // acceptする度にlenが書き換わるので毎回初期化
        len = (socklen_t) sizeof(from);
        /* 接続受付 */
        // accにクライアントへのsocketが返る
        // fromには受け付けたソケットのアドレス情報が入る
        if ((acc = accept(soc, (struct sockaddr *) &from, &len)) == -1) {
            if(errno != EINTR) {
                perror("accept");
            }
        } else {
            (void) getnameinfo((struct sockaddr *) &from, len,
                            hbuf, sizeof(hbuf),
                            sbuf, sizeof(sbuf),
                            NI_NUMERICHOST | NI_NUMERICSERV);
            (void) fprintf(stderr, "accept:%s:%s\n", hbuf, sbuf);
            /* 送受信ループ */
            send_recv_loop(acc);
            /* アクセプトソケットクローズ */
            (void) close(acc);
            acc = 0;
        }
    }
}

/* サイズ指定文字列連結 */
// sizeはコピー先バッファのサイズを想定
size_t
mystrlcat(char *dst, const char *src, size_t size)
{
    const char *ps;
    char *pd, *pde;
    size_t dlen, lest;

    // コピー先文字列の現在の終端まで移動
    // 終端>sizeの場合はコピーせず終了
    for (pd = dst, lest = size; *pd != '\0' && lest != 0; pd++, lest--);
    dlen = pd - dst;
    if (size - dlen == 0) {
        return (dlen + strlen(src));
    }
    // コピー先バッファの終端位置
    pde = dst + size - 1;
    // srcをコピーしきるかバッファの終端に達するまでコピー
    for (ps = src; *ps != '\0' && pd < pde; pd++, ps++) {
       *pd = *ps;
    }
    // コピー先バッファの残りを\0で埋める
    for (; pd <= pde; pd++) {
        *pd = '\0';
    }
    while (*ps++);
    return (dlen + (ps - src - 1));
}

/* 送受信ループ */
void
send_recv_loop(int acc)
{
    char buf[512], *ptr;
    ssize_t len;
    // 1クライアントとの送受信ループ
    // 1つのクライアントが切断される間で他のクライアントは待たされる
    for (;;) {
        /* 受信 */
        if ((len = recv(acc, buf, sizeof(buf), 0)) == -1) {
            /* エラー */
            perror("recv");
            break;
        }
        if (len == 0) {
            /* EOF */
            (void) fprintf(stderr, "recv:EOF\n");
            break;
        }
        /* 文字列化・表示 */
        buf[len] = '\0';
        if((ptr = strpbrk(buf, "\r\n")) != NULL) {
            *ptr = '\0';
        }
        (void) fprintf(stderr, "[client]%s\n", buf);
        /* 応答文字列作成 */
        (void) mystrlcat(buf, ":OK\r\b", sizeof(buf));
        len = (ssize_t) strlen(buf);
        if ((len = send(acc, buf, (size_t) len, 0)) == -1) {
            /* エラー */
            perror("send");
            break;
        }
    }
}

int
main(int argc, char* argv[])
{
    int soc;
    /* 引数にポート番号が指定されているか？ */
    if (argc <= 1) {
        (void) fprintf(stderr, "server port\n");
        return (EX_USAGE);
    }
    /* サーバーソケットの準備 */
    if((soc = server_socket(argv[1])) == -1) {
        (void) fprintf(stderr, "server_socket(%s):error\n", argv[1]);
        return (EX_UNAVAILABLE);
    }
    (void) fprintf(stderr, "ready for accept\n");
    /* アクセプトループ */
    accept_loop(soc);
    /* ソケットクローズ */
    // 実際はaccept_loopがCtrl+Cしないと止まらないのでここには到達しない
    (void) close(soc);
    return (EX_OK);
}

