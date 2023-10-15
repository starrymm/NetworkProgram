#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "Server.h"

#define MAX_BUFFER_SIZE 1024*1024

Server::Server() {
    m_sendBuffer = NULL;
    m_receiveBuffer = NULL;
    m_serverSocket = -1;
    m_threadId = 0;
    m_isRunning = true;
    m_maxSocketId = -1;
    m_clientArrayPos = 0;
    memset(m_clientSocket, 0, sizeof(m_clientSocket));
}

Server::~Server() {


}

int Server::init(const char *ip, int port) {
    if (m_sendBuffer == NULL) {
        char *tmp = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
        if (!tmp) {
            printf("failed to malloc buffer\n"); 
            return -1;
        }
        m_sendBuffer = tmp;
    }

    if (m_receiveBuffer == NULL) {
        char *tmp = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
        if (!tmp) {
            printf("failed to malloc buffer\n"); 
            return -1;
        }
        m_receiveBuffer = tmp;
    }

    // create socket
    // bind
    // listen
    // accept
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket == -1)  {
        printf("create server socket failed\n");
        return -1;
    }

    int flag = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1) {
        printf("set socket reuseaddr failed [%d],socket:%d",errno,m_serverSocket);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    int ret = 0;
    ret = bind(m_serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("failed to bind\n");
        return -1;
    }

    ret = listen(m_serverSocket, 5);
    if (ret < 0) {
        printf("failed to listen\n");
        return -1;
    }
    m_maxSocketId = m_serverSocket;
    
    if (m_threadId == 0) {
        int ret = pthread_create(&m_threadId, NULL, serverFun, this);
    }

    return 0;
}

void *Server::serverFun(void *arg) {
    Server *imp = (Server *)arg;
    imp->run();
    return NULL;
}

int Server::readNBytes(int fd, char *buf, int bytes){
    if (bytes <= 0) {
        return bytes;
    }

    int nleft, nread;
    char *ptr = buf;
    nleft = bytes;
    while (nleft > 0) {
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
            int _errno = errno;
            printf("read data(%d) nleft(%d) failed[%d:%s]\n", bytes, nleft, _errno, strerror(_errno));
            if (_errno == EINTR || _errno == EAGAIN) {
                nread = 0;
                usleep(30000);
            } else {
                printf("read byts(%d) nread from(%d) failed, errno:%d\n", bytes, nread, fd, _errno);
                return -1;
            }
        } else if (nread == 0) {
            printf("read data from fd(%d) is zero, errno:%d\n", fd, errno);
            break;
        }
        nleft -= nread;
        ptr += nread;
    }
    return bytes-nleft;
}

int Server::writeNBytes(int fd,const char *buffer,int bytes){
    if(bytes <= 0){
        printf("write bytes(%d) to fd(%d) is not legal\n",bytes,fd);
        return bytes;
    }
    int bytes_left = bytes,written_bytes = 0;
    const char *ptr = buffer ;
    while(bytes_left > 0){
        //write data
        written_bytes = write(fd, ptr, bytes_left);
        if(written_bytes <= 0){
            //  iterrupt by system call
            if(errno == EINTR){
                printf("write data to socket encounter EINTR , continue\n");
                continue ;
            }else if(errno == EAGAIN){
                //  time out
                //LOGI("write errno = EAGAIN,left:%d,socet:%d",bytes - bytes_left,fd);
                usleep(2000);
                continue ;
            }else{
                printf("write bytes(%d) to (fd:%d) failed,errno:%d\n",bytes,fd,errno);
                return -1;
            }
        }
        bytes_left -= written_bytes;
        ptr += written_bytes;
    }
    return bytes;
}

int Server::processSingleClient(int &client_socket, fd_set *fds) {
    if (!(client_socket > 0 && FD_ISSET(client_socket, fds))) {
        return 0;
    }

    //ioctl FIONREAD:returns the number of bytes available to read from an
    //               inotify file descriptor.
    //获取可读数据的字节大小
    int bytes = 0;
    int ret = ioctl(client_socket, FIONREAD, &bytes);
    if (ret == -1) {
        printf("Server", "ioctl failed to manipulate socket fd[%d], errno[%d], strerr[%s]\n", client_socket, errno, strerror(errno));
        FD_CLR(client_socket, fds);
        close(client_socket);
        client_socket = -1;
        return -1;
    }

    if (bytes == 0) {
        printf("client socket:%d shut down\n", client_socket);
        FD_CLR(client_socket, fds);
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    memset(m_receiveBuffer, 0, MAX_BUFFER_SIZE); 
    ret = readNBytes(client_socket, m_receiveBuffer, bytes);

    ret = writeNBytes(client_socket, m_receiveBuffer, bytes); 
    
    return 0;
}

int Server::processClientIO(fd_set *fds) {
    for (int i = 0; i < MAX_SOCKET_NUM; i++){
        processSingleClient(m_clientSocket[i], fds);
    }
    return 0;
}

int Server::processServerIO(fd_set *fds){
    if (!( m_serverSocket > 0 && FD_ISSET(m_serverSocket, fds))) {
        return 0;
    }
    
    struct sockaddr_in client_addr;
    socklen_t sin_size = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    int client_socket = accept(m_serverSocket, (struct sockaddr *)&client_addr, &sin_size);
    if (client_socket == -1) {
        printf("failed to accept new connection from client, errno:%d, strerr[%s]", errno, strerror(errno)); 
        return -1;
    }
    if (m_maxSocketId < client_socket) {
        m_maxSocketId = client_socket;
    }

    int pos = m_clientArrayPos % MAX_SOCKET_NUM;
    m_clientSocket[pos] = client_socket;
    m_clientArrayPos = ++ pos;
    return 0;
}


int Server::runOnce() {
    fd_set fds;    
    FD_ZERO(&fds);
    setFd(&fds);       

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    int ret = select(m_maxSocketId + 1, &fds, NULL, NULL, &tv);
    if (ret == 0) {
        printf("select timeout\n");
    } else if (ret < 0) {
        printf("select error:%d, msg:%s", errno, strerror(errno));
        return 0;
    }

    ret = processClientIO(&fds); 

    ret = processServerIO(&fds);
    if (ret != 0) {
        printf("failed to process server socket I/O operation, socketfd[%d]", m_serverSocket);
    }

    return 0;
}

void Server::run() {
    while(m_isRunning) {
        int ret = runOnce();
    }
}

void Server::setFd(fd_set *fds) {
    if (m_serverSocket > 0) {
        FD_SET(m_serverSocket, fds);
    }
    for (int i = 0; i < MAX_SOCKET_NUM; i++) {
        if (m_clientSocket[i] > 0) {
            FD_SET(m_clientSocket[i], fds);
        }
    }

    return ;
}