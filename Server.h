#ifndef __SERVER_H__
#define __SERVER_H__
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SOCKET_NUM 10
class Server {
    
public:
    Server();
    ~Server();

public:
    int init(const char *ip, int port);

private:
    static void *serverFun(void *arg);
    void run();
    int runOnce();
    void setFd(fd_set *fds);
    int processServerIO(fd_set *fds);
    int processClientIO(fd_set *fds);
    int processSingleClient(int &client_socket, fd_set *fds);
    int readNBytes(int fd, char *buf, int bytes);
    int writeNBytes(int fd,const char *buffer,int bytes);

private:
    char *m_sendBuffer;
    char *m_receiveBuffer;
    int m_serverSocket;
    int m_maxSocketId;
    int m_clientSocket[MAX_SOCKET_NUM];
    int m_clientArrayPos;
    bool m_isRunning;
    pthread_t m_threadId;
};


#endif