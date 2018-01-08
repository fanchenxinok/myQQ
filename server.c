#include "userList.h"
#include "sqlite3_userInfo.h"
#include "messagePacker.h"
#include <sys/epoll.h>

#define portnumber 3333
#define MAXCLIENTNUM 100  //�������ӵ����ͻ���
#define MAXSIZE 512
#define RTSP_SERVER_PORT "8554"
static char buffer[MAXSIZE];
static int userNumber = 0;
static int udpServerPid = -1;
static int rtsp_server_pid = -1;
static int rtsp_client_num = 0;

static UL* userHead = NULL; //�û�����ͷ

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
                usleep(5000);  /* ˯��5 MS */
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
                usleep(5000);  /* ˯��5 MS */
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
    /* Ⱥ����Ϣ */
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
        waitpid(*pPid, NULL, 0); /* һ��Ҫwait ��Ȼ�������ʬ�߳� */
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
    /* Ⱥ����Ϣ */
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

                /* �ȼ���û��Ƿ��Ѿ���¼ */
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
                /* ����û��Ƿ���� */
                /* ����û����������Ƿ�ƥ�� */
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
				if(res == QQ_OK){ //������ͬ�����޸�
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

                /* �㲥�û�ע������Ϣ */
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

                /* �㲥�û��˳���¼����Ϣ */
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
                /* �пͻ�����Ҫͬ��������Ƶ */
                if(rtsp_server_pid == -1){ /* ��һ������ */
                    rtsp_server_pid = startRtspSendMediaServer();
                    if(rtsp_server_pid != -1 && pNode->nodeInfo.rtspPlayFlag != 1){
                        sendMsgToUser(sockfd, MSG_SERVER_MEDIA_PLAYING, RTSP_SERVER_PORT);
                        pNode->nodeInfo.rtspPlayFlag = 1;
                        rtsp_client_num++;
                        printf("####### rtsp server is running[clients:%d]............\n", rtsp_client_num);
                    }
                }
                else{ /*�����ͻ�������*/
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
    fflush(stdin); //�建��
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
        /* �����û�����Ϊ�ǵ�¼״̬ */
        //setLoginByName(userHead, pNode->nodeInfo.userId, LOGIN_NO);
        setLoginBySockID(userHead, userFd, LOGIN_NO);
        
        printf("*<After>*:\n");
        showAllUsers(userHead);
        printf("left %d users online.....\n", numberOfOnlineUsers(userHead));

        /* �㲥�û��˳�����Ϣ */
        memset(msg.msgText, '\0', MAXSIZE);
        sprintf(msg.msgText, "User %s has exit!!", pNode->nodeInfo.userId);
        brocastMessage(userHead, msg.msgText, userFd);
        brocastOnlineUserInfo(userHead, userFd);

        if(pNode->nodeInfo.rtspPlayFlag == 1){
            if(--rtsp_client_num == 0 
                && rtsp_server_pid != -1){ /* ������еĿͻ��˶������˹ر�rtsp������ */
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

    signal(SIGPIPE,SIG_IGN);  //����ͻ��������˳����·������˳�������

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

    /* ����߳��յ�sigpipe �ź��˳������� */
    sigset_t signal_mask;
    sigemptyset (&signal_mask);
    sigaddset (&signal_mask, SIGPIPE);
    int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0) {
        printf("block sigpipe error/n");
    } 

    /* �������˿�ʼ����sockfd������ */ 
    if((serverSockfd = socket(AF_INET,SOCK_STREAM,0))==-1){ // AF_INET:IPV4;SOCK_STREAM:TCP
        fprintf(stderr,"Socket error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 
    printf("Socket success.....\n");
    
    /* ����������� sockaddr�ṹ */ 
    bzero(&server_addr,sizeof(struct sockaddr_in)); // ��ʼ��,��0
    server_addr.sin_family=AF_INET;                 // Internet
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);  // (���������ϵ�long����ת��Ϊ�����ϵ�long����)���κ�����ͨ��  //INADDR_ANY ��ʾ���Խ�������IP��ַ�����ݣ����󶨵����е�IP
    //server_addr.sin_addr.s_addr=inet_addr("192.168.1.1");  //���ڰ󶨵�һ���̶�IP,inet_addr���ڰ����ּӸ�ʽ��ipת��Ϊ����ip
    server_addr.sin_port=htons(portnumber);         // (���������ϵ�short����ת��Ϊ�����ϵ�short����)�˿ں�

    int n = 1;
    setsockopt(serverSockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(int));//����ʹ�÷������Ͽ����Ͼ�����������

    /* ����sockfd��������IP��ַ */ 
    printf("start bindding.....\n");
    if(bind(serverSockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1){ 
        fprintf(stderr,"Bind error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 

    /* �����������ӵ����ͻ����� */ 
    printf("start listening.....\n");
    printf("The max number of client to allow to connect with server is:%d.....\n", MAXCLIENTNUM);
    if(listen(serverSockfd, MAXCLIENTNUM)==-1){ 
        fprintf(stderr,"Listen error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 

    printf("Tcp server working.....\n");

#if USE_SQLITE3_DB
    /* ���ݿ⼰���ݱ�Ĵ��� */
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
        /* ��ȡ�������������ַ */
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
        /* ����UDP ������ */
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
		        printf("@@@Server get connection from %s, socketFd = %d\n", inet_ntoa(client_addr.sin_addr), new_fd); // �������ַת����.�ַ���

				epollAddEvent(epollFd, new_fd, EPOLLIN);
			}
			else{
				if(events[i].events & EPOLLIN){
		        	epollProcess(epollFd, events[i].data.fd);
				}
			}
		}
    }

    /* ����ͨѶ */
	close(epollFd);
    close(serverSockfd); 
    freeUserList(userHead);
    closeChildProcess(&udpServerPid);
    myQQ_DeInit();
    exit(0); 
} 
