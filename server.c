#include "userList.h"
#include "sqlite3_userInfo.h"
#include "messagePacker.h"
#include <sys/epoll.h>

#define portnumber 3333
#define MAXCLIENTNUM 100  //运行连接的最大客户量
#define MAXSIZE 512
#define RTSP_SERVER_PORT "8554"
static char buffer[MAXSIZE];
static int userNumber = 0;
static int udpServerPid = -1;
static int rtsp_server_pid = -1;
static int rtsp_client_num = 0;

static UL* userHead = NULL; //用户链表头

#define USE_SQLITE3_DB (1)

#define STR(a) #a
#define TO_STR(a) STR(a)

#define STR_LINK(a, b)  a ## b
#define THREAD_NAME_MAKE(a, b) STR_LINK(a, b)

static void epollAddEvent(int epollFd, int addFd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = addFd;
	epoll_ctl(epollFd, EPOLL_CTL_ADD, addFd, &ev);
	return;
}

static void epollDeleteEvent(int epollFd, int addFd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = addFd;
	epoll_ctl(epollFd, EPOLL_CTL_DEL, addFd, &ev);
	return;
}

static void epollModifyEvent(int epollFd, int addFd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = addFd;
	epoll_ctl(epollFd, EPOLL_CTL_MOD, addFd, &ev);
	return;
}

static void sendMsgToUser(int socketFd, emMsgType msgType, const char* msgText)
{
    msg_st msg_send;
    msg_send.msgType = msgType;
    if(msgText != NULL){
        memset(msg_send.msgText, '\0', MAXSIZE);
        sprintf(msg_send.msgText, "%s", msgText);
    }
    write(socketFd, &msg_send, sizeof(msg_st));
    return;
}

static void searchAndReply(const char* name, int socketFd)
{
    UL* pFind = findNodeByName(userHead, name);
    msg_st msg;
    msg.msgType = MSG_BROCAST;
    if(pFind){
        sprintf(msg.msgText, "User %s has %s.", name, pFind->nodeInfo.logFlag == 1 ? "LOGIN" : "LOGOUT");
        write(socketFd, &msg, sizeof(msg_st));
    }
    else{
        sprintf(msg.msgText, "User %s is not exist.", name);
        write(socketFd, &msg, sizeof(msg_st));
    }
    return;
}

static void sendUserAcountInfo(int sockfd, int ReOnline)
{
    msg_st msg_send;
    if(numberOfList(userHead) > 0){
        UL* p = userHead->next;
        if(ReOnline == 1){
            msg_send.msgType = MSG_FLASH_OL_USERS;
            while(p != NULL){
                if(p->nodeInfo.logFlag != 1){
                    p = p->next;
                    continue;
                }
                
                memset(msg_send.name, '\0', 20);
                strcpy(msg_send.name, p->nodeInfo.userId);
                write(sockfd, &msg_send, sizeof(msg_st));
                printf("******name: %s *******\n", msg_send.name);
                p = p->next;
                usleep(5000);  /* 睡眠5 MS */
            }
        }
        else{
            msg_send.msgType = MSG_FLASH_ALL_USERS;
            int n = 1;
            while(p != NULL){
                memset(msg_send.name, '\0', 20);
                strcpy(msg_send.name, p->nodeInfo.userId);
                sprintf(msg_send.msgText, "     User <%d>: <name: %s, login: %s>.", \
                    n, p->nodeInfo.userId, (p->nodeInfo.logFlag == 1 ? "YES" : "NO"));
                write(sockfd, &msg_send, sizeof(msg_st));
                printf("******name: %s *******login: %d ********\n", msg_send.name, p->nodeInfo.logFlag);
                p = p->next;
                n++;
                usleep(5000);  /* 睡眠5 MS */
            }
        }
    }
    else{
        msg_send.msgType = MSG_BROCAST;
        sprintf(msg_send.msgText, "%s", "There have no user in the list!!!.\n");
        write(sockfd, &msg_send, sizeof(msg_st));
        return;
    }
    printf("brocast user list over!\n");
    return;
}

static void brocastOnlineUserInfo(UL* head, int cur_fd)
{
    /* 群发消息 */
    UL* pCur = head->next;
    while(pCur){
        if((pCur->nodeInfo.socketfd == cur_fd) ||
            (pCur->nodeInfo.logFlag != 1)){
            pCur = pCur->next;
            continue;
        }
        
        sendUserAcountInfo(pCur->nodeInfo.socketfd, 1);
        pCur = pCur->next;
    }
    return;
}

static void closeChildProcess(int *pPid)
{
    if(*pPid != -1){
        kill(*pPid, SIGKILL);
        waitpid(*pPid, NULL, 0); /* 一定要wait 不然会产生僵尸线程 */
        *pPid = -1;
    }
    return;
}

static int startRtspSendMediaServer()
{
    int rtspServerPid = -1;
    if((rtspServerPid = fork()) == 0){
        if(execl("./rtsp_sender", "rtsp_sender", "-p", RTSP_SERVER_PORT, (char*)0) < 0){
            printf("execl rtsp_sender error!!!!!\n");
            return -1;
        }
    }
    return rtspServerPid;
}

static void brocastMessage(UL* head, const char* msg, const int cur_fd)
{
    /* 群发消息 */
    msg_st msg_send;
    msg_send.msgType = MSG_BROCAST;
    strcpy(msg_send.msgText, msg);
    msg_send.msgText[strlen(msg)] = '\0';
    UL* pCur = head->next;
    while(pCur){
        if((pCur->nodeInfo.socketfd == cur_fd) ||
            (pCur->nodeInfo.logFlag == 0)){
            pCur = pCur->next;
            continue;
        }
        
        write(pCur->nodeInfo.socketfd, &msg_send, sizeof(msg_st));
        pCur = pCur->next;
    }
    return;
}


static void handleMessage(int sockfd, msg_st *msg)
{
    if(!msg)
        return;
    char name[20], passward[20];

	UL *pNode = NULL;
	if(msg->msgType != MSG_USER_REGIST && 
		msg->msgType !=  MSG_USER_LOGIN_CHECK){
		pNode = findNodeBySocketID(userHead, sockfd);
		if(pNode == NULL){
			printf("[tcp server] can not find node by sockfd: %d in list!!!!!\n", sockfd);
			return;
		}
	}

	printf("[tcp server] handleMessage()->sockfd = %d\n", sockfd);
    
    switch(msg->msgType){
        case MSG_USER_REGIST:
            {
                memset(name, '\0', sizeof(char) * 20);
                memset(passward, '\0', sizeof(char) *20);
                char c = ';';
                char* p = strchr(msg->msgText, c);
                strncpy(name, msg->msgText, p - msg->msgText);
                name[p - msg->msgText] = '\0';
                strcpy(passward, p + 1);
                passward[strlen(passward)] = '\0';		
                printf("[tcp server]Register: User name: %s, passward: %s\n", name, passward);
                EM_RES res = myQQ_Register(name, passward);
                if(res == QQ_USER_EXIST){
                    sendMsgToUser(sockfd, MSG_USER_EXIST, NULL);
                    printf("[tcp server]regist user has exist!!!!\n");
                }
                else if(res == QQ_OK){
                    sendMsgToUser(sockfd, MSG_USER_REGIST_OK, NULL);
					UIfo newUser;
					memset(&newUser, 0, sizeof(UIfo));
			        newUser.socketfd = sockfd;
			        newUser.logFlag = 0;
			        newUser.rtspPlayFlag = 0;
                    strncpy(newUser.userId, name, strlen(name) + 1);
                    printf("[tcp server]regist user success!!!!\n");
                    addUserToList(userHead, newUser);
                }
                else{
                    sendMsgToUser(sockfd, MSG_FAIL, NULL);
                    printf("[tcp server]regist user fail!!!!\n");
                }
            }
            break;
        case MSG_USER_LOGIN_CHECK:
            {
                memset(name, '\0', sizeof(char) * 20);
                memset(passward, '\0', sizeof(char) *20);
                char c = ';';
                char* p = strchr(msg->msgText, c);
                strncpy(name, msg->msgText, p - msg->msgText);
                name[p - msg->msgText] = '\0';
                strcpy(passward, p + 1);
                passward[strlen(passward)] = '\0';		
                printf("[tcp server]Login: User name: %s, passward: %s\n", name, passward);

                /* 先检查用户是否已经等录 */
                UL *pNode = findNodeByName(userHead, name);
				if(pNode == NULL){
					sendMsgToUser(sockfd, MSG_USER_UN_EXIST, NULL);
                    printf("[tcp server]Login user %s is not exist!!!!\n", name);
					return;
				}
				
                if((pNode != NULL) && (pNode->nodeInfo.logFlag == 1)){
                    sendMsgToUser(sockfd, MSG_USER_HAS_ONLINE, NULL);
                    printf("[tcp server]Login user %s has online!!!!\n", name);
                    return;
                }
                /* 检查用户是否存在 */
                /* 检查用户名和密码是否匹配 */
                EM_RES res = myQQ_LoginCheck(name, passward);
                
                if(res == QQ_USER_UN_EXIST){
                    sendMsgToUser(sockfd, MSG_USER_UN_EXIST, NULL);
                    printf("[tcp server]Login user %s is not exist!!!!\n", name);
                }
                else if(res == QQ_PASSWARD_WRONG){
                    sendMsgToUser(sockfd, MSG_USER_LOGIN_FAIL, NULL);
                    printf("[tcp server]Login fail!!!!\n");
                }
                else if(res == QQ_OK){
                    sendMsgToUser(sockfd, MSG_USER_LOGIN_OK, NULL);
                    pNode->nodeInfo.logFlag = 1;
                    pNode->nodeInfo.socketfd = sockfd;
                    
                    printf("[tcp server]Login success!!!!\n");

                    char msgText[MAXSIZE];
                    sprintf(msgText, "User %s has login, all online uses as following:", name);
                    brocastMessage(userHead, msgText, sockfd);
                    brocastOnlineUserInfo(userHead, sockfd);
                }
                else{
                    printf("[tcp server]Login fail!!!!\n");
                }
            }
            break;
        case MSG_REQ_OL_USERS:
            sendUserAcountInfo(sockfd, 1);
            break;
        case MSG_REQ_ALL_USERS:
            sendUserAcountInfo(sockfd, 0);
            break;
        case MSG_SEARCH_SOMEONE:
            searchAndReply(msg->name, sockfd);
            break;
        case MSG_USER_RENAME:
            {
                printf("[tcp server]User %s want to change name to %s.\n", pNode->nodeInfo.userId, msg->name);
                msg_st msg_send;
                if(msg->name[0] == '\0'){
                    sendMsgToUser(sockfd, MSG_BROCAST, "Server say: name can not be null!!!");
                    return;
                }
                
                EM_RES res = myQQ_ChangeName(pNode->nodeInfo.userId, msg->name);
                if(res != QQ_OK){
                    printf("[tcp server]change user name in database fail!!!!\n");
                    return;
                }

                UL* my = findNodeByName(userHead, pNode->nodeInfo.userId);
                if(my){
                    strncpy(my->nodeInfo.userId, msg->name, strlen(msg->name) +1);
                }
                else{
                    printf("[tcp server] can not find %s in userHead list\n", pNode->nodeInfo.userId);
                    myQQ_ChangeName(msg->name, pNode->nodeInfo.userId);
                    return;
                }
                
                strncpy(pNode->nodeInfo.userId, msg->name, strlen(msg->name) +1);
                printf("[tcp server]change online user name done!!!!\n");

                msg_send.msgType = MSG_USER_RENAME_OK;
                strncpy(msg_send.name, msg->name, strlen(msg->name) +1);
                write(sockfd, &msg_send, sizeof(msg_st));
                printf("[tcp server] change user name success!!!!\n");
            }
            break;
        case MSG_USER_CHG_PW:
            {
                if(msg->msgText[0] == '\0'){
                    sendMsgToUser(sockfd, MSG_BROCAST, "Server say: passward can not be null!!!");
                    return;
                }
				EM_RES res = myQQ_IsSamePassward(pNode->nodeInfo.userId, msg->msgText);
				if(res == QQ_OK){ //密码相同不用修改
					printf("[tcp server] the new passward is the same to old one!!!\n");
					sendMsgToUser(sockfd, MSG_BROCAST, "the new passward is the same to old one, no need change!!!");
					return;
				}
				
                res = myQQ_ChangePassward(pNode->nodeInfo.userId, msg->msgText);
                if(res != QQ_OK){
                    printf("[tcp server]change user passward in database fail!!!!\n");
                    return;
                }
                printf("[tcp server] change user passward to (%s) success!!!!\n", msg->msgText);
                sendMsgToUser(sockfd, MSG_BROCAST, "Server say: change passward success!!!");
            }
            break;
        case MSG_USER_UNREGIST:
            {
                printf("[tcp server] handing user %s unregister request!!\n", pNode->nodeInfo.userId);
                EM_RES res = myQQ_UnRegister(pNode->nodeInfo.userId);
                if(res != QQ_OK){
                    sendMsgToUser(sockfd, MSG_BROCAST, "Server say: unregister fail!!!");
                    return;
                }

				printf("[tcp server] UnRegister (%s) success!!!!\n", pNode->nodeInfo.userId);
                deleteNodeByName(userHead, pNode->nodeInfo.userId);
                sendMsgToUser(sockfd, MSG_USER_UNREGIST_OK, NULL);

                /* 广播用户注销的消息 */
                char msgText[MAXSIZE];
                memset(msgText, '\0', MAXSIZE);
                sprintf(msgText, "User %s has unregister!!", pNode->nodeInfo.userId);
                brocastMessage(userHead, msgText, pNode->nodeInfo.socketfd);
                brocastOnlineUserInfo(userHead, pNode->nodeInfo.socketfd);
            }
            break;
       case MSG_USER_LOGOUT:
            {
                if(pNode){
					printf("[tcp server] handing user %s logout request!!\n", pNode->nodeInfo.userId);
                    pNode->nodeInfo.logFlag = 0;
                    sendMsgToUser(sockfd, MSG_USER_LOGOUT_OK, NULL);
                }
                else{
                    sendMsgToUser(sockfd, MSG_BROCAST, "server say: logout fail because of you are not in user list!!\n");
                }

                /* 广播用户退出登录的消息 */
                char msgText[MAXSIZE];
                memset(msgText, '\0', MAXSIZE);
                sprintf(msgText, "User %s has logout!!!", pNode->nodeInfo.userId);
                brocastMessage(userHead, msgText, pNode->nodeInfo.socketfd);
                brocastOnlineUserInfo(userHead, pNode->nodeInfo.socketfd);
                printf("[tcp server] User (%s) logout success!!!!\n", pNode->nodeInfo.userId);
            }
            break;
        case MSG_USER_REQ_MEDIA:
            {
                /* 有客户端需要同步播放视频 */
                if(rtsp_server_pid == -1){ /* 第一次请求 */
                    rtsp_server_pid = startRtspSendMediaServer();
                    if(rtsp_server_pid != -1 && pNode->nodeInfo.rtspPlayFlag != 1){
                        sendMsgToUser(sockfd, MSG_SERVER_MEDIA_PLAYING, RTSP_SERVER_PORT);
                        pNode->nodeInfo.rtspPlayFlag = 1;
                        rtsp_client_num++;
                        printf("####### rtsp server is running[clients:%d]............\n", rtsp_client_num);
                    }
                }
                else{ /*其他客户端请求*/
                    if(pNode->nodeInfo.rtspPlayFlag != 1){
                        sendMsgToUser(sockfd, MSG_SERVER_MEDIA_PLAYING, RTSP_SERVER_PORT);
                        pNode->nodeInfo.rtspPlayFlag = 1;
                        rtsp_client_num++;
                        printf("####### rtsp server is running[clients:%d]............\n", rtsp_client_num);
                    }
                }
            }
            break;
		case MSG_USER_STOP_PLAY_MEDIA:
            {
                if(pNode->nodeInfo.rtspPlayFlag == 1){
                    pNode->nodeInfo.rtspPlayFlag = 0;
                    if(--rtsp_client_num == 0 &&
                        rtsp_server_pid != -1){
                        closeChildProcess(&rtsp_server_pid);
                        printf("******* rtsp server is closed************\n");
                    }
                }
                else{
                    printf("user: %s had not open rtsp client to play share media.\n", pNode->nodeInfo.userId);
                }
                printf("&&&  rtsp client number = %d\n", rtsp_client_num);
            }
            break;  
        default:
            if(msg->msgText[0] != '\0'
                && msg->msgText[0] != '\n'){
                //printf("msg->msgText[0]= %c, msgType = %d\n", msg->msgText[0],msg->msgType);
                printf("User %s say: %s\n", pNode->nodeInfo.userId, msg->msgText);
            }
            break;
    }
    return;
}

static void epollProcess(int epollFd, int userFd)
{	
    int nbytes;
    msg_st msg;
    fflush(stdin); //清缓存
    memset(&msg, 0, sizeof(msg_st));	

    if((nbytes = read( userFd, &msg, sizeof(msg_st))) == -1){ 
        fprintf(stderr,"Read Error:%s\n",strerror(errno));
        return;
    }
    
    if((nbytes == 0) || (msg.msgType == MSG_USER_QUIT)){
		UL *pNode = NULL;
		pNode = findNodeBySocketID(userHead, userFd);
		if(pNode == NULL){
			setLoginBySockID(userHead, userFd, LOGIN_NO);
			epollDeleteEvent(epollFd, userFd, EPOLLIN);
			return;
		}
		epollDeleteEvent(epollFd, userFd, EPOLLIN);
        printf("pUser->logFlag = %d\n", pNode->nodeInfo.logFlag);
        if(pNode->nodeInfo.logFlag != 1) return;
        printf("User %s has exit!, sockfd = %d\n", pNode->nodeInfo.userId, userFd);
        printf("*<Before>*:\n");
        showAllUsers(userHead);
        /* 将该用户设置为非登录状态 */
        //setLoginByName(userHead, pNode->nodeInfo.userId, LOGIN_NO);
        setLoginBySockID(userHead, userFd, LOGIN_NO);
        
        printf("*<After>*:\n");
        showAllUsers(userHead);
        printf("left %d users online.....\n", numberOfOnlineUsers(userHead));

        /* 广播用户退出的消息 */
        memset(msg.msgText, '\0', MAXSIZE);
        sprintf(msg.msgText, "User %s has exit!!", pNode->nodeInfo.userId);
        brocastMessage(userHead, msg.msgText, userFd);
        brocastOnlineUserInfo(userHead, userFd);

        if(pNode->nodeInfo.rtspPlayFlag == 1){
            if(--rtsp_client_num == 0 
                && rtsp_server_pid != -1){ /* 如果所有的客户端都结束了关闭rtsp服务器 */
                closeChildProcess(&rtsp_server_pid);
                printf("******* rtsp server is closed************\n");
            }
            pNode->nodeInfo.rtspPlayFlag = 0;
        }                        

        printf("&&&  rtsp client number = %d\n", rtsp_client_num);
        return;
    }

    handleMessage(userFd, &msg);
	printf("[tcp server] epoll process end!\n");
    return;
}


int main(int argc, char *argv[]) 
{ 
    int serverSockfd,new_fd; 
    struct sockaddr_in server_addr; 
    struct sockaddr_in client_addr; 
    int sin_size; 
    pid_t pid;

    system("clear");

    signal(SIGPIPE,SIG_IGN);  //解决客户端意外退出导致服务器退出的问题

    #if 0
    char hostname[255];
    gethostname(hostname, sizeof(hostname));
    printf("%s\n", hostname);
    struct hostent *hent = gethostbyname(hostname);
    if(hent){
        int i = 0;
        for(i = 0; hent->h_addr_list[i]; i++) {
            printf("%s\n", inet_ntoa(*(struct in_addr*)(hent->h_addr_list[i])));
        }
    }
    #endif

    /* 解决线程收到sigpipe 信号退出的问题 */
    sigset_t signal_mask;
    sigemptyset (&signal_mask);
    sigaddset (&signal_mask, SIGPIPE);
    int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0) {
        printf("block sigpipe error/n");
    } 

    /* 服务器端开始建立sockfd描述符 */ 
    if((serverSockfd = socket(AF_INET,SOCK_STREAM,0))==-1){ // AF_INET:IPV4;SOCK_STREAM:TCP
        fprintf(stderr,"Socket error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 
    printf("Socket success.....\n");
    
    /* 服务器端填充 sockaddr结构 */ 
    bzero(&server_addr,sizeof(struct sockaddr_in)); // 初始化,置0
    server_addr.sin_family=AF_INET;                 // Internet
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);  // (将本机器上的long数据转化为网络上的long数据)和任何主机通信  //INADDR_ANY 表示可以接收任意IP地址的数据，即绑定到所有的IP
    //server_addr.sin_addr.s_addr=inet_addr("192.168.1.1");  //用于绑定到一个固定IP,inet_addr用于把数字加格式的ip转化为整形ip
    server_addr.sin_port=htons(portnumber);         // (将本机器上的short数据转化为网络上的short数据)端口号

    int n = 1;
    setsockopt(serverSockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(int));//这样使得服务器断开马上就能重新启动

    /* 捆绑sockfd描述符到IP地址 */ 
    printf("start bindding.....\n");
    if(bind(serverSockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1){ 
        fprintf(stderr,"Bind error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 

    /* 设置允许连接的最大客户端数 */ 
    printf("start listening.....\n");
    printf("The max number of client to allow to connect with server is:%d.....\n", MAXCLIENTNUM);
    if(listen(serverSockfd, MAXCLIENTNUM)==-1){ 
        fprintf(stderr,"Listen error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 

    printf("Tcp server working.....\n");

#if USE_SQLITE3_DB
    /* 数据库及数据表的创建 */
    if(myQQ_Init() != QQ_OK){
        printf("myQQ_Init() FAIL !!!!\n");
        exit(-1);
    }

    /*test for sqlite db*/
#if 0
    myQQ_Init();
    myQQ_Register("001", "123");
    myQQ_Register("002", "456");
    myQQ_Register("003", "789");
    myQQ_LoginCheck("004", "123");
    myQQ_LoginCheck("001", "456");
    myQQ_LoginCheck("002", "456");
    myQQ_ChangeName("003", "004");
    myQQ_ChangePassward("004", "111");
    myQQ_DeInit();
#endif
#endif

    if((udpServerPid = fork()) == 0){
        /* 读取服务器的网络地址 */
        int fd = open("serverIp.txt", O_RDONLY);
        if(fd == -1){
            printf("open serverIp.txt error!\n");
            exit(-1);
        }
        
        int iplen = 0;
        char ip[16];
        if((iplen = read(fd, ip, 16)) == -1){
            printf("read serverIp.txt error!\n");
            exit(-1);
        }
        
        ip[iplen] = '\0';
        /* 开启UDP 服务器 */
        const char* portNum_udp = "8888";
        //printf("udpServerPid = %d, tcpServerPid = %d, ip =%s, portNum = %s\n", udpServerPid, getppid(), ip, portNum_udp);
        if(execl("./udp_server", "udp_server", ip, portNum_udp, (char*)0) < 0){
            printf("execl udp_server error!!!!!\n");
        }
    }

    printf("udpServerPid = %d, tcpServerPid = %d\n", udpServerPid, getpid());

    userHead = initUserList();
    myQQ_GetAllUsers(userHead);
    printf("All users in database as follow:\n");
    showAllUsers(userHead);

	int epollFd = epoll_create(MAXCLIENTNUM);
	epollAddEvent(epollFd, serverSockfd, EPOLLIN);
    while(1)
    {
		struct epoll_event events[MAXCLIENTNUM];
		memset(events, 0, sizeof(struct epoll_event) * MAXCLIENTNUM);
		printf("[tcp server] epoll waiting......\n");
		int eventNum = epoll_wait(epollFd, events, MAXCLIENTNUM, -1);
		printf("[tcp server] epoll wake up, event num = %d!\n", eventNum);
		int i = 0;
		for(; i < eventNum; i++){
			if((events[i].data.fd == serverSockfd) && 
				(events[i].events & EPOLLIN)){
		        sin_size=sizeof(struct sockaddr_in); 
		        if((new_fd = accept(serverSockfd,(struct sockaddr *)(&client_addr),&sin_size))==-1){ 
		            fprintf(stderr,"Accept error:%s\n\a",strerror(errno)); 
		            close(serverSockfd); 
		            freeUserList(userHead);
		            closeChildProcess(&udpServerPid);
		            myQQ_DeInit(); 
		            exit(-1);
		        } 
		        printf("@@@Server IP: %s\n",inet_ntoa(server_addr.sin_addr));
		        printf("@@@Server get connection from %s, socketFd = %d\n", inet_ntoa(client_addr.sin_addr), new_fd); // 将网络地址转换成.字符串

				epollAddEvent(epollFd, new_fd, EPOLLIN);
			}
			else{
				if(events[i].events & EPOLLIN){
		        	epollProcess(epollFd, events[i].data.fd);
				}
			}
		}
    }

    /* 结束通讯 */
	close(epollFd);
    close(serverSockfd); 
    freeUserList(userHead);
    closeChildProcess(&udpServerPid);
    myQQ_DeInit();
    exit(0); 
} 
