#include <signal.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

// volatile sig_atmic_tで宣言された0-127の数値は
// シグナルハンドラの非同期処理中でも安全に扱えることが保証されている
// よってこのg_gotsigを終了フラグとして利用する
volatile sig_atomic_t g_gotsig = 0;

/* SIGINTハンドラ */
void
sig_int_handler(int sig)
{
    // 終了フラグを立てるだけ
    g_gotsig = 1;
}

// 他のシグナルもデフォルトの動作が強制終了のものがあるので
// 無視に設定して奥野が一般的らしい
// （daemon化していれば実際問題になるケースは少ないらしい）

// signalでの実装
void
ignore_signals()
{
    (void) signal(SIGPIPE, SIG_IGN);
    (void) signal(SIGUSR1, SIG_IGN);
    (void) signal(SIGUSR2, SIG_IGN);
    (void) signal(SIGTTIN, SIG_IGN);
    (void) signal(SIGTTOU, SIG_IGN);
}

// sigactionでの実装
#define IGNORE_SIGACTION(SIGNAME) \
    (void) sigaction(SIGNAME, (struct sigaction *) NULL, &sa);\
    sa.sa_handler = SIG_IGN;\
    sa.sa_flags = SA_NODEFER;\
    (void) sigaction(SIGNAME, &sa, (struct sigaction *) NULL);\

void
ignore_signals_by_action()
{
    struct sigaction sa;
    IGNORE_SIGACTION(SIGPIPE);
    IGNORE_SIGACTION(SIGUSR1);
    IGNORE_SIGACTION(SIGUSR2);
    IGNORE_SIGACTION(SIGTTIN);
    IGNORE_SIGACTION(SIGTTOU);
}

int
main(int argc, char *argv[])
{
    // sigactionでシグナルを制御
    struct sigaction sa;
    (void) sigaction(SIGINT, (struct sigaction *) NULL, &sa);
    sa.sa_handler = sig_int_handler;
    sa.sa_flags = SA_NODEFER;
    (void) sigaction(SIGINT, &sa, (struct sigaction *) NULL);

#ifdef IGN_SIG
    (void) ignore_signals();
#elif IGN_ACTION
    (void) ignore_signals_by_action();
#endif

    // 1秒ごとに"."を表示
    while (g_gotsig == 0) {
        (void) fprintf(stderr, ".");
        sleep(1);
    }

    // Ctrl+Cまたはkill -SIGINT PID を実行しても
    // すぐに終了せずこの行も実行される
    (void) fprintf(stderr, "\nEND\n");
    return (EX_OK);
}

