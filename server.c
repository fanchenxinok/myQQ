#include "userList.h"
#include "sqlite3_userInfo.h"
#include "messagePacker.h"
#include <sys/epoll.h>

#define portnumber 3333
#define MAXCLIENTNUM 100  //�������ӵ����ͻ���
#define MAXSIZE 1024
#define RTSP_SERVER_PORT "8554"
static char buffer[MAXSIZE];
static int userNumber = 0;
static int udpServerPid = -1;
static int rtsp_server_pid = -1;
static int rtsp_client_num = 0;

UL* userHead = NULL; //�û�����ͷ

#define USE_SQLITE3_DB (1)

#define STR(a) #a
#define TO_STR(a) STR(a)

#define STR_LINK(a, b)  a ## b
#define THREAD_NAME_MAKE(a, b) STR_LINK(a, b)

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
                
                memset(msg_send.name, '\0', 24);
                memset(msg_send.userIp, '\0', 16);
                strcpy(msg_send.name, p->nodeInfo.userId);
                strcpy(msg_send.userIp, p->nodeInfo.ip);
                write(sockfd, &msg_send, sizeof(msg_st));
                printf("******name: %s *******ip: %s ********\n", msg_send.name, msg_send.userIp);
                p = p->next;
                usleep(5000);  /* ˯��5 MS */
            }
        }
        else{
            msg_send.msgType = MSG_FLASH_ALL_USERS;
            int n = 1;
            while(p != NULL){
                memset(msg_send.name, '\0', 24);
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

#if NEED_SAVE_LOGIN_FLAG
static void setAllUserLogout(UL* head)
{
    if(!head) return;
    UL* pCur = head->next;
    while(pCur){
        myQQ_SetUserLogout(pCur->nodeInfo.userId);
        pCur = pCur->next;
    }
    return;
}
#endif

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


static void handleMessage(int sockfd, msg_st *msg, UIfo *user_info)
{
    if(!msg || !user_info)
        return;
    char name[24], passward[24];
    
    switch(msg->msgType){
        case MSG_USER_REGIST:
            {
                memset(name, '\0', sizeof(char) * 24);
                memset(passward, '\0', sizeof(char) *24);
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
                    strncpy(user_info->userId, name, strlen(name) + 1);
                    printf("[tcp server]regist user success!!!!\n");
                    user_info->logFlag = 0;
                    addUserToList(userHead, *user_info);
                }
                else{
                    sendMsgToUser(sockfd, MSG_FAIL, NULL);
                    printf("[tcp server]regist user fail!!!!\n");
                }
            }
            break;
        case MSG_USER_LOGIN_CHECK:
            {
                memset(name, '\0', sizeof(char) * 24);
                memset(passward, '\0', sizeof(char) *24);
                char c = ';';
                char* p = strchr(msg->msgText, c);
                strncpy(name, msg->msgText, p - msg->msgText);
                name[p - msg->msgText] = '\0';
                strcpy(passward, p + 1);
                passward[strlen(passward)] = '\0';		
                printf("[tcp server]Login: User name: %s, passward: %s\n", name, passward);

                /* �ȼ���û��Ƿ��Ѿ���¼ */
                UL *pNode = findNodeByName(userHead, name);
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
                    printf("[tcp server]Login user %s is not regist!!!!\n", name);
                }
                else if(res == QQ_PASSWARD_WRONG){
                    sendMsgToUser(sockfd, MSG_USER_LOGIN_FAIL, NULL);
                    printf("[tcp server]Login fail!!!!\n");
                }
                else if(res == QQ_OK){
                    sendMsgToUser(sockfd, MSG_USER_LOGIN_OK, NULL);
                    strncpy(user_info->userId, name, strlen(name) + 1);
                    if(pNode == NULL){
                        user_info->logFlag = 1;
                        addUserToList(userHead, *user_info);
                    }
                    else{
                        pNode->nodeInfo.logFlag = 1;
                        user_info->logFlag = 1;
                        pNode->nodeInfo.socketfd = user_info->socketfd;
                        strcpy(pNode->nodeInfo.ip, user_info->ip);
                        #if NEED_SAVE_LOGIN_FLAG
                        myQQ_SetUserLogin(name);
                        #endif
                    }
                    
                    printf("[tcp server]Login success!!!!\n");

                    char msgText[1024];
                    sprintf(msgText, "User %s has login, all online uses as following:", user_info->userId);
                    brocastMessage(userHead, msgText, user_info->socketfd);
                    brocastOnlineUserInfo(userHead, user_info->socketfd);
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
        case MSG_USER_STOP_PLAY_MEDIA:
            {
                if(user_info->rtspPlayFlag == 1){
                    user_info->rtspPlayFlag = 0;
                    if(--rtsp_client_num == 0 &&
                        rtsp_server_pid != -1){
                        closeChildProcess(&rtsp_server_pid);
                        printf("******* rtsp server is closed************\n");
                    }
                }
                else{
                    printf("user: %s had not open rtsp client to play share media.\n", user_info->userId);
                }
                printf("&&&  rtsp client number = %d\n", rtsp_client_num);
            }
            break;  
        case MSG_USER_RENAME:
            {
                printf("[tcp server]User %s want to change name to %s.\n", user_info->userId, msg->name);
                msg_st msg_send;
                if(msg->name[0] == '\0'){
                    sendMsgToUser(sockfd, MSG_BROCAST, "Server say: name can not be null!!!");
                    return;
                }
                
                EM_RES res = myQQ_ChangeName(user_info->userId, msg->name);
                if(res != QQ_OK){
                    printf("[tcp server]change user name in database fail!!!!\n");
                    return;
                }

                UL* my = findNodeByName(userHead, user_info->userId);
                if(my){
                    strncpy(my->nodeInfo.userId, msg->name, strlen(msg->name) +1);
                }
                else{
                    printf("[tcp server] can not find %s in userHead list\n", user_info->userId);
                    myQQ_ChangeName(msg->name, user_info->userId);
                    return;
                }
                
                strncpy(user_info->userId, msg->name, strlen(msg->name) +1);
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
                EM_RES res = myQQ_ChangePassward(user_info->userId, msg->msgText);
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
                printf("[tcp server] handing user %s unregister request!!\n", user_info->userId);
                EM_RES res = myQQ_UnRegister(user_info->userId);
                if(res != QQ_OK){
                    sendMsgToUser(sockfd, MSG_BROCAST, "Server say: unregister fail!!!");
                    return;
                }

                deleteNodeByName(userHead, user_info->userId);
                sendMsgToUser(sockfd, MSG_USER_UNREGIST_OK, NULL);
                printf("[tcp server] UnRegister (%s) success!!!!\n", user_info->userId);

                /* �㲥�û�ע������Ϣ */
                char msgText[MAXSIZE];
                memset(msgText, '\0', MAXSIZE);
                sprintf(msgText, "User %s has unregister!!", user_info->userId);
                brocastMessage(userHead, msgText, user_info->socketfd);
                brocastOnlineUserInfo(userHead, user_info->socketfd);
            }
            break;
       case MSG_USER_LOGOUT:
            {
                printf("[tcp server] handing user %s logout request!!\n", user_info->userId);
                UL *pNode = findNodeByName(userHead, user_info->userId);
                if(pNode){
                    #if NEED_SAVE_LOGIN_FLAG
                    myQQ_SetUserLogout(user_info->userId);
                    #endif
                    pNode->nodeInfo.logFlag = 0;
                    sendMsgToUser(sockfd, MSG_USER_LOGOUT_OK, NULL);
                }
                else{
                    printf("[tcp server] handing user logout can not find %s in user's list\n", user_info->userId);
                    sendMsgToUser(sockfd, MSG_BROCAST, "server say: logout fail because of you are not in user list!!\n");
                }

                /* �㲥�û��˳���¼����Ϣ */
                char msgText[MAXSIZE];
                memset(msgText, '\0', MAXSIZE);
                sprintf(msgText, "User %s has logout!!!", user_info->userId);
                brocastMessage(userHead, msgText, user_info->socketfd);
                brocastOnlineUserInfo(userHead, user_info->socketfd);
                printf("[tcp server] User (%s) logout success!!!!\n", user_info->userId);
            }
            break;
        case MSG_USER_REQ_MEDIA:
            {
                /* �пͻ�����Ҫͬ��������Ƶ */
                if(rtsp_server_pid == -1){ /* ��һ������ */
                    rtsp_server_pid = startRtspSendMediaServer();
                    if(rtsp_server_pid != -1 && user_info->rtspPlayFlag != 1){
                        sendMsgToUser(sockfd, MSG_SERVER_MEDIA_PLAYING, RTSP_SERVER_PORT);
                        user_info->rtspPlayFlag = 1;
                        rtsp_client_num++;
                        printf("####### rtsp server is running[clients:%d]............\n", rtsp_client_num);
                    }
                }
                else{ /*�����ͻ�������*/
                    if(user_info->rtspPlayFlag != 1){
                        sendMsgToUser(sockfd, MSG_SERVER_MEDIA_PLAYING, RTSP_SERVER_PORT);
                        user_info->rtspPlayFlag = 1;
                        rtsp_client_num++;
                        printf("####### rtsp server is running[clients:%d]............\n", rtsp_client_num);
                    }
                }
            }
            break;
        default:
            if(msg->msgText[0] != '\0'
                && msg->msgText[0] != '\n'){
                //printf("msg->msgText[0]= %c, msgType = %d\n", msg->msgText[0],msg->msgType);
                printf("User %s say: %s\n", user_info->userId, msg->msgText);
            }
            break;
    }
    return;
}

static void Process(void *arg)
{	
    pthread_t pdt = pthread_self();
    char pname[10];
    int a = 100;
    pthread_setname_np(pdt, TO_STR(THREAD_NAME_MAKE(pth_, process)));
    pthread_getname_np(pdt, pname);
    printf("----------pthread <%s> starting... Thread ID: %ld-----------\n", pname, pdt);

    fd_set readfd;
    int nbytes;
    UIfo* pUser = (UIfo*)arg;
    UIfo user;
    memcpy(&user, pUser, sizeof(UIfo));  //������
    pUser = &user;

    /* TCP �����߳̿�ʼ��� */
    msg_st msg;
    fflush(stdin); //�建��
    while(1){
        FD_ZERO(&readfd);
        FD_SET(pUser->socketfd, &readfd);
        FD_SET(0, &readfd);
        int i, maxfd = pUser->socketfd + 1;
        int r = select(maxfd, &readfd, NULL, NULL, NULL);
        if(r >= 0){
            //������������Ϣ
            if(FD_ISSET( pUser->socketfd, &readfd)){
                sem_wait(&pUser->sem); //��ȡ�ź���
                memset(&msg, 0, sizeof(msg_st));
                if((nbytes = read( pUser->socketfd, &msg, sizeof(msg_st))) == -1){ 
                    fprintf(stderr,"Read Error:%s\n",strerror(errno));
                    sem_post(&pUser->sem);  //һ��Ҫ�ͷ��ź���
                    break;
                }
                
                if((nbytes == 0) || (msg.msgType == MSG_USER_QUIT)){
                    printf("pUser->logFlag = %d\n", pUser->logFlag);
                    if(pUser->logFlag != 1) break;
                    printf("User %s has exit!, sockfd = %d\n", pUser->userId, pUser->socketfd);
                    printf("*<Before>*:\n");
                    showAllUsers(userHead);
                    /* �����û�����Ϊ�ǵ�¼״̬ */
                    #if NEED_SAVE_LOGIN_FLAG
                    EM_RES res = myQQ_SetUserLogout(pUser->userId);  
                    if(res != QQ_OK){
                        printf("myQQ_SetUserUnlogin() fail!!\n");
                    }
                    #endif
                    setLoginByName(userHead, pUser->userId, LOGIN_NO);
                    
                    printf("*<After>*:\n");
                    showAllUsers(userHead);
                    printf("left %d users online.....\n", numberOfOnlineUsers(userHead));

                    /* �㲥�û��˳�����Ϣ */
                    memset(msg.msgText, '\0', MAXSIZE);
                    sprintf(msg.msgText, "User %s has exit!!", pUser->userId);
                    brocastMessage(userHead, msg.msgText, pUser->socketfd);
                    brocastOnlineUserInfo(userHead, pUser->socketfd);

                    if(pUser->rtspPlayFlag == 1){
                        if(--rtsp_client_num == 0 
                            && rtsp_server_pid != -1){ /* ������еĿͻ��˶������˹ر�rtsp������ */
                            closeChildProcess(&rtsp_server_pid);
                            printf("******* rtsp server is closed************\n");
                        }
                        pUser->rtspPlayFlag = 0;
                    }                        

                    printf("&&&  rtsp client number = %d\n", rtsp_client_num);
                    sem_post(&pUser->sem);  //һ��Ҫ�ͷ��ź���
                    break;
                }

                handleMessage(pUser->socketfd, &msg, pUser);
                
                sem_post(&pUser->sem); //�ͷ��ź���
            }
        }
        else{
            //em_post(user->sem); //�ͷ��ź���
            printf("select error!\n");
            break;
        }
        usleep(50000); //ʵ��һ�Զ�ͨ��
    }
    
    FD_CLR(pUser->socketfd, &readfd);
    close(pUser->socketfd); 
    printf("----------pthread <Name: %s> <ID: %ld> is dead----------\n", pname, pdt);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{ 
    int sockfd,new_fd; 
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
    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){ // AF_INET:IPV4;SOCK_STREAM:TCP
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
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(int));//����ʹ�÷������Ͽ����Ͼ�����������

    /* ����sockfd��������IP��ַ */ 
    printf("start bindding.....\n");
    if(bind(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1){ 
        fprintf(stderr,"Bind error:%s\n\a",strerror(errno)); 
        exit(1); 
    } 

    /* �����������ӵ����ͻ����� */ 
    printf("start listening.....\n");
    printf("The max number of client to allow to connect with server is:%d.....\n", MAXCLIENTNUM);
    if(listen(sockfd, MAXCLIENTNUM)==-1){ 
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

    while(1)
    {
        sin_size=sizeof(struct sockaddr_in); 
        if((new_fd=accept(sockfd,(struct sockaddr *)(&client_addr),&sin_size))==-1){ 
            fprintf(stderr,"Accept error:%s\n\a",strerror(errno)); 
            close(sockfd); 
            freeUserList(userHead);
            closeChildProcess(&udpServerPid);
            myQQ_DeInit(); 
            exit(-1);
        } 
        fprintf(stderr,"@@@Server IP: %s\n",inet_ntoa(server_addr.sin_addr));
        fprintf(stderr,"@@@Server get connection from %s, socketFd = %d\n", inet_ntoa(client_addr.sin_addr), new_fd); // �������ַת����.�ַ���


        /* �����߳̽��е�¼��⼰�������� */
        UIfo newUser;
        memset(&newUser, 0, sizeof(UIfo));
        newUser.socketfd = new_fd;
        newUser.logFlag = 0;
        newUser.rtspPlayFlag = 0;
        int iplen = strlen(inet_ntoa(client_addr.sin_addr));
        strncpy(newUser.ip, inet_ntoa(client_addr.sin_addr), iplen);
        newUser.ip[iplen] = '\0';
        sem_init(&newUser.sem, 0, 1); //��ʼ���ź���
        pthread_t process_pid;
        int err = pthread_create(&process_pid, NULL, (void*)Process, (void*)&newUser);
        if(err != 0){
            printf("process pthread_create error!\n");
            close(new_fd);
        }
    }

    /* ����ͨѶ */
    #if NEED_SAVE_LOGIN_FLAG
    setAllUserLogout(userHead);  /* �����ݿ��״̬��� */
    #endif
    close(sockfd); 
    freeUserList(userHead);
    closeChildProcess(&udpServerPid);
    myQQ_DeInit();
    exit(0); 
} 
