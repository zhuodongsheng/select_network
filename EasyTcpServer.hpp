#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_
#include "MessageHeader.hpp"

#ifdef _WIN32
    #include <windows.h>
    #include <WinSock2.h>
    #pragma comment(lib, "ws2_32.lib");
    #define WIN32_LEAN_AND_MEAN
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
#else
    #include <unistd.h>  // uninx std
    #include <arpa/inet.h>
    #include <string.h>

    #define SOCKET int
    #define INVALID_SOCKET  (SOCKET)(~0)
    #define SOCKET_ERROR    (-1)   
#endif


#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE  10240
#endif

#include <stdio.h>
#include <vector>
#include <thread>

class ClientSocket
{
public:
    ClientSocket(SOCKET sockfd = INVALID_SOCKET)
    {
        _sockfd = sockfd;
        memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
        _lastPos = 0;
    }

    SOCKET sockfd()
    {
        return _sockfd;
    }

    char* msgBuf()
    {
        return _szMsgBuf;
    }

    int getLastPos()
    {
        return _lastPos;
    }

    void setLastPos(int pos)
    {
        _lastPos = pos;
    }
private:
    SOCKET _sockfd; //socket fd_set ,file desc set文件描述符集合
    //消息缓冲区
    char _szMsgBuf[RECV_BUFF_SIZE * 10] = {}; 
    //消息缓冲区数据尾部
    int _lastPos = 0;

};

class EasyTcpServer
{
    SOCKET _sock;
    std::vector<ClientSocket*> _clients;

private:

public:
    EasyTcpServer()
    {
        _sock = INVALID_SOCKET;
    }

    virtual ~EasyTcpServer()
    {
        Close();
    }

    //初始化socket
    SOCKET InitSocket()
    {
        //启动Win sock2.x环境
#ifdef _WIN32
        WORD ver = MAKEWORD(2, 2);
        WSADATA dat;
        WSAStartup(ver, &dat);
#endif
        //1 建立一个socket
        if(INVALID_SOCKET != _sock)
        {
            printf("<socket=%d>关闭旧连接。。。\n", _sock);
            Close();
        }

        _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        if(INVALID_SOCKET == _sock)
        {
            printf("错误， 建立套接字失败。。。\n");
        }
        else
        {
            printf("建立套接字Socket=<%d>成功。。。\n", _sock);
        }

    }
    //绑定IP and 端口号
    int Bind(const char* ip, unsigned short port)
    {
        if(INVALID_SOCKET == _sock)
        {
            InitSocket();
        }
        //2.bind 绑定用于接受客户端链接的网络端口
        sockaddr_in _sin = {};
        _sin.sin_family = AF_INET;
        _sin.sin_port = htons(port);//host to net unsigned short
#ifdef _WIN32
        if(ip)
            _sin.sin_addr.S_un.s_addr = inet_addr(ip);
        else
            _sin.sin_addr.S_un.s_addr = INADDR_ANY;
#else
        if(ip)
            _sin.sin_addr.s_addr = inet_addr(ip);
        else
            _sin.sin_addr.s_addr = INADDR_ANY;
#endif
        int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
        if (SOCKET_ERROR == ret)
        {
            printf("ERROR, 绑定网络端口<%d>失败。。。\n",port);
        }
        else
        {
            printf("绑定网络端口<%d>成功...\n", port);
        }

        return ret;
    }
    //监听端口
    int Listen(int n)
    {
        //3 listen 监听网络端口
        int ret = listen(_sock, n);
        if(SOCKET_ERROR == ret)
        {
            printf("Socket=<%d>错误， 绑定网络端口失败。。。\n", _sock);
        }
        else
        {
            printf("Socket=<%d>绑定网络端口成功。。。\n", _sock);
        }
        return ret;
    }
    //客户端连接
    SOCKET Accept()
    {
                    //4 accept 等待接受的客户端链接
        sockaddr_in clientAddr = {};
        int nAddrLen = sizeof(sockaddr_in);
        SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
        cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
        cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
        if(INVALID_SOCKET == cSock)
        {
            printf("Socket=<%d>错误，接收到无效客户端SOCKET。。。\n", _sock);
        }
        else
        {
           
            NewUserJoin userJoin;
            SendDataToALL(_sock, &userJoin);
            _clients.push_back(new ClientSocket(cSock));
            printf("Socket=<%d>新客户端加入：socket = %d, IP = %s\n", 
                _sock, (int)cSock, inet_ntoa(clientAddr.sin_addr));
        }

        return cSock;
        

    }
    //关闭socket连接
    //关闭网络
    void Close()
    {
        //关闭Win sock2.x环境
        //4 关闭套接字 closesocket
        if(_sock != INVALID_SOCKET)
        {
#ifdef _WIN32
            for(int n=0; n<_clients.size(); n++)
            {
                closesocket(_clients[n]->sockfd());
                delete _clients[n];
            }
            //6 关闭套接字closesocket
            WSACleanup();
#else
            for(int n=0; n<_clients.size(); n++)
                {
                    close(_clients[n]->sockfd());
                    delete _clients[n];
                }
            close(_sock);
#endif      
            _clients.clear();
        }
        _sock = INVALID_SOCKET;
    }

    //处理网络消息
    int _ncount = 0;
    bool OnRun()
    {
        if(isRun())
        {
                        // 伯克利 socket
            fd_set fdRead;
            fd_set fdWrite;
            fd_set fdExp;

            FD_ZERO(&fdRead); //清空
            FD_ZERO(&fdWrite);
            FD_ZERO(&fdExp);

            FD_SET(_sock, &fdRead);
            FD_SET(_sock, &fdWrite);
            FD_SET(_sock, &fdExp);
            SOCKET maxSock = _sock;

            for(int64_t n=0; n<_clients.size(); n++)
            {
                FD_SET(_clients[n]->sockfd(), &fdRead);
                if(maxSock < _clients[n]->sockfd())
                {
                    maxSock = _clients[n]->sockfd();
                }
            }
            //nfds 是一个整数，描述socket的范围而不是数量
            timeval t = {1, 0};
            int ret = select(maxSock+1, &fdRead, &fdWrite, &fdExp, &t);
            // printf("select ret = %d count=%d\n", ret, _ncount++);
            if(ret < 0)
            {
                printf("select 任务结束\n");
                Close();
                return false;
            }
            if(FD_ISSET(_sock, &fdRead))
            {
                FD_CLR(_sock,  &fdRead);
                Accept();
            }

            for(int n = (int)_clients.size() - 1; n>=0; n--)
            {
                if(FD_ISSET(_clients[n]->sockfd(), &fdRead))
                {
                    if(-1 == RecvData(_clients[n]))
                    {
                        auto iter = _clients.begin() + n;
                        if(iter != _clients.end())
                        {
                            delete _clients[n];
                            _clients.erase(iter);
                        }
                    }
                }
            }   
        }
    }

    //是否工作中
    bool isRun()
    {
        return _sock != INVALID_SOCKET;
    }

    //接受数据 处理粘包 拆分宝
    char _szRecv[RECV_BUFF_SIZE] = {};
    int RecvData(ClientSocket* pClient)
    {
        int nLen = recv(pClient->sockfd(), _szRecv, sizeof(_szRecv), 0);

        if(nLen <= 0)
        {
            printf("客户端<Socket = %d>已推出， 任务结束。\n", pClient->sockfd());
            return -1;
        }
        memcpy(pClient->msgBuf()+ pClient->getLastPos(), _szRecv, nLen);
        //消息缓冲区的数据尾部位置后移
        pClient->setLastPos(pClient->getLastPos() + nLen);
        //判断消息缓冲区的数据长度是否大于消息头
        while(pClient->getLastPos() >= sizeof(DataHeader))
        {
            //这时就可以知道当前消息体的长度
            DataHeader* header = (DataHeader*)pClient->msgBuf();
            if(pClient->getLastPos() > header->dataLength)
            {
                //剩余未处理消息缓冲区数据的长度
                int nSize = pClient->getLastPos() - header->dataLength;
                //处理网络消息
                OnNetMsg(pClient->sockfd(), header);
                //将剩余未处理消息缓冲区数据前移
                memcpy(pClient->msgBuf(), pClient->msgBuf()+header->dataLength, nSize);
                pClient->setLastPos(nSize);;
            }
            else
            {   
                //剩余数据不够一条完整消息
                break;
            }
            
        }
        return 0;
    }

    //响应网络数据
    virtual void OnNetMsg(SOCKET cSock, DataHeader* header)
    {
    switch(header->cmd)
    {
        case CMD_LOGIN:
        {
            Login* login = (Login*)header;
            // printf("收到<Socket = %d>命令：CMD_LOGIN 数据长度：%d  userName=%s PassWord=%s\n",
            //         cSock, login->dataLength, login->userName, login->passWord);

            //判断用户密码是否正确
            // LoginResult ret;
            // SendData(cSock, &ret);
        }
        break;
        case CMD_LOGOUT:
        {   
            Logout* logout = (Logout*) header;
            // printf("收到<Socket = %d>命令：CMD_LOGOUT 数据长度：%d\n",
                    // cSock, logout->dataLength);
            //判断用户密码是否正确
            // LogoutResult ret;
            // SendData(cSock, &ret);
        }
        break;
        default:
        {
            // DataHeader header;
            // SendData(cSock, &header);
            printf("收到<Socket = %d>未定义消息 数据长度：%d\n",
                    cSock, header->dataLength);
            
        }
        break;
        }
    }

    //发送指定socket数据
    int SendData(SOCKET cSock, DataHeader* header)
    {
        if(isRun() && header)
        {
            return send(cSock, (const char*)header, header->dataLength, 0);
        }
        return SOCKET_ERROR;
    }

        //发送指定socket数据
    void  SendDataToALL(SOCKET cSock, DataHeader* header)
    {
        for(int64_t n=0; n<_clients.size(); n++)
        {
            SendData(_clients[n]->sockfd(), header);
        }
    }


};

#endif