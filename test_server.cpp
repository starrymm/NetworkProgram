#include <stdio.h>
#include "Server.h"

int main(int argc, char *argv[]) {
    Server *imp = new Server();
    const char *ip = "127.0.0.1";
    int port = 8000;
    int ret = imp->init(ip, port);
    if (ret == 0) {
        printf("init success\n");
    } else {
        printf("init faield\n");
    }
    while (1) {
        sleep(100000000);
    }

    return 0;
}