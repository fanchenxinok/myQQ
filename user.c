#include "userList.h"
#include "messagePacker.h"

#define portnumber 3333   //用来连接服务器
#define userPortNumber "8888"  //UDP端口用来和其他客户端通信的
#define MAXSIZE 1024

UL* onlineUser = NULL;
static char myname[MAXSIZE];
static char server_ip[30];

static int rtspClientPid = -1;

enum{
    LOGIN_NO = 0,
    LOGIN_YES = 1,
    UN_REGIST_NO = 0,
    UN_REGIST_YES = 1
};

enum{
    RES_FAIL = -1,
    RES_OK,
    RES_GO_LOGIN,
    RES_GO_REGIST,
    RES_GO_EXIT
};

static int s_loginFlag = LOGIN_NO;
static int s_unRegistFlag = UN_REGIST_NO;

char myGetChar()
{
    char c;
    while(1){
        c = getchar();
        if(c == '\0' || c == '\n')
            continue;
        else
            break;
    }
    return c;
}

static void sendMsgToServer(int serverSocketFd, emMsgType msgType, const char* msgText)
{
    msg_st msg_send;
    msg_send.msgType = msgType;
    if(msgText != NULL){
        memset(msg_send.msgText, '\0', MAXSIZE);
        sprintf(msg_send.msgText, "%s", msgText);
    }
    write(serverSocketFd, &msg_send, sizeof(msg_st));
    return;
}


int UserLogin(int serverSocket)
{
    printf("You have 3 chances to login\n");
    int n = 3;
    while(n--)
    {
        char name[MAXSIZE], passward[MAXSIZE];
        printf("*0*0*0*Please input your UserName: ");
        memset(name, '\0', sizeof(name));
        scanf("%s", name); 
        printf("*0*0*0*Please input your Passward: ");
        memset(passward, '\0', sizeof(passward));
        scanf("%s", passward);
        msg_st msg;
        msg.msgType = MSG_USER_LOGIN_CHECK;
        memset(msg.msgText, '\0', MAXSIZE * sizeof(char));
        sprintf(msg.msgText, "%s;%s", name, passward);
        write(serverSocket, &msg, sizeof(msg_st));  
        usleep(500000);

        memset(&msg, 0, sizeof(msg_st));
        read(serverSocket, &msg, sizeof(msg_st));
        if(msg.msgType == MSG_USER_LOGIN_OK){
            printf("User %s Login success....\n", name);
            getchar(); //去除scanf留下的回车符
            memset(myname, 0, sizeof(myname));
            strcpy(myname, name);
            return RES_OK;
        }
        else if(msg.msgType == MSG_USER_UN_EXIST){
            printf("User name: %s is not exist, you can regist a new user.\n", name);
            printf("if you want to register a new user, input 'r' after input 'y'.\n");
            printf("Please input (y/n) >: ");
            char c;
            c = myGetChar();
            
            if(c == 'y'){
                return RES_GO_REGIST;
            }
            else{
                printf("User name: %s is not exist, please input again!\n", name);
            }
        }
        else if(msg.msgType == MSG_USER_HAS_ONLINE){
            printf("User %s has login, please don't login again!\n", name);
        }
        else{
            printf("User name and passward is not match, please input again\n");
        }
    }
    printf("Login faild\n");
    return RES_FAIL;
}

int UserRegister(int serverSocket)
{
    printf("Warning: User name and user passward must less than 24 char....\n");
    char name[24], passward[24], check[24];
    printf("*^*^*^*Please input your UserName: ");
    memset(name, '\0', sizeof(name));
    scanf("%s", name); 
    int n = 3;
    while(n--){
        printf("*^*^*^*Please input your Passward: ");
        memset(passward, '\0', sizeof(passward));
        scanf("%s", passward);
        printf("*^*^*^*Please input your Passward again: ");
        memset(check, '\0', sizeof(check));
        scanf("%s", check);
        if(strncmp(passward, check, 24) != 0){
            printf("input passward are not the same, please input again!!!!!\n");
        }
        else{
            break;
        }
    }

    if(n <= 0) return RES_FAIL;
    msg_st msg;
    msg.msgType = MSG_USER_REGIST;
    memset(msg.msgText, '\0', 1024 * sizeof(char));
    sprintf(msg.msgText, "%s;%s", name, passward);
    write(serverSocket, &msg, sizeof(msg_st));
    usleep(500000);
    
    read(serverSocket, &msg, sizeof(msg_st));
    if(msg.msgType == MSG_USER_REGIST_OK){
    	printf("register user %s success....\n", name);
    	getchar(); //去除scanf留下的回车符
    	memset(myname, 0, sizeof(myname));
    	strcpy(myname, name);
    	return RES_OK;
    }
    else if(msg.msgType == MSG_USER_EXIST){
    	printf("User %s has exist, please don't resgist again\n", name);
       return RES_GO_LOGIN;
    }
    else{
    	printf("Register user %s fail!!!\n", name);
    }
    return RES_FAIL;
}

void startTalkToOtherUsers()
{
    if(fork() == 0){
        printf("execl myname = %s, server_ip=%s, userPortNumber = %d\n", myname, server_ip, atoi(userPortNumber));
        if(execl("./udp_client", "udp_client", server_ip, userPortNumber, myname, (char*)0) < 0){
            printf("execl udp_client error!!!!!!!\n");
            return;
        }
    }
    
    wait(NULL);
    return;
}

int startPlayShareMedia(const char* portNumber)
{
    int child_pid = -1;
    if((child_pid = fork()) == 0){
        char rtspLocation[256] = {'\0'};
        sprintf(rtspLocation, "rtsp://%s:%s/test", server_ip, portNumber);
        printf("rtsp location = %s\n", rtspLocation);
        if(execl("./rtsp_receiver", "rtsp_receiver", "-l", rtspLocation, (char*)0) < 0){
            printf("execl rtsp_receiver error!!!!!!!\n");
            return -1;
        }
    }
    return child_pid;
}

static void alarm_fun()
{
    if(s_loginFlag == LOGIN_NO){
        printf("login time out\n");
        alarm(0);
        exit(0);
    }
    else{
        alarm(0);
    }
    return;
}

void menu1()
{
    printf("please choose the following command:\n");
    printf("		[q] User to quit\n");
    printf("		[l] User to login\n");
    printf("		[r] User to register\n");
    printf(">>: ");
}

void getCommand1(int sockfd)
{
    char c;
    while(1)
    {
        c = myGetChar();
        
        if(c == '\n') continue;
        switch(c){
            case 'q':
            {
                close(sockfd);
                exit(0);
            }
            case 'l':
            {
                printf("Please Login...\n");
                signal(SIGALRM, alarm_fun); //等待信号到来
                alarm(60); //定时60秒，为了防止客户端太久不登录
                //用户登录
                int res = UserLogin(sockfd);
                if(res == RES_FAIL){
                    printf("UserLogin return LOGIN_FAIL....\n");
                    	close(sockfd);
                	exit(0);
                }
                else if(res == RES_GO_REGIST){
                    printf("UserLogin return LOGIN_GO_REGIST....\n");
                    printf("Please input 'r' >: ");
                    break;
                }
                else{
                    printf("UserLogin return LOGIN_OK....\n");
                	s_loginFlag = LOGIN_YES;
                	alarm(0);
                	signal(SIGALRM, SIG_IGN); //如果登录成功就忽略定时信号
                	return;
                }	
            }
                break;
            case 'r':
            {
                printf("Please Register a new count....\n");
                signal(SIGALRM, alarm_fun); //等待信号到来
                alarm(300); //定时300秒，为了防止客户端太久不登录
                //用户登录
                int rec = UserRegister(sockfd);
                if(rec == RES_FAIL){
                     close(sockfd);
                	exit(0);
                }
                else if(rec == RES_GO_LOGIN){  //用户已经注册
                    printf("Do you want to login??? please input (y/n): \n");
                    c = myGetChar();
                    if(c == 'n'){
                        return;
                    }
                    if(UserLogin(sockfd) == -1){
                    	    close(sockfd);
                	    exit(0);
                    }
                    else{
                        s_loginFlag = LOGIN_YES;
                        s_unRegistFlag = UN_REGIST_NO;
                        alarm(0);
                        signal(SIGALRM, SIG_IGN); //如果登录成功就忽略定时信号
                        return;
                    }	
                }
                else{
                     s_unRegistFlag = UN_REGIST_NO;
                	alarm(0);
                	signal(SIGALRM, SIG_IGN); //如果注册成功就忽略定时信号
                	return;
                }	
            }
                break;
            default:
                printf("Command not in the list, please input the right command>>:");
                break;
        }
    }
    return;
}

void menu2()
{
    printf("<<<< commands >>>>:\n");
    printf("		[cmd] you can choose some comand to excute.\n");
    printf("		[quit] User to quit\n");
}

void menu3()
{
    printf("please choose the command:\n");
    printf("		[f] you can flash the online user list!\n");
    printf("		[u] you can talk with one user!\n");
    printf("		[r] you can change your name!\n");
    printf("		[c] you can change your passward!\n");
    printf("		[d] you can unregister your count!\n");
    printf("		[o] you can logout and return login UI.\n");
    printf("		[a] show all users(online and not online).\n");
    printf("		[s] you can search someone by name.\n");
    printf("		[p] play media stream what the server is playing!\n");
    printf("		[t] stop playing media stream what the server is playing!\n");
    printf("		[q] give up choose command!\n");
    
    printf(">>: ");
}

char getCommand3(int sockfd)
{
    char c = '\n';
    while(1){
        c = getchar();
        if(c == '\n') continue;
        break;
    }
    
    switch(c){
        case 'f':
            sendMsgToServer(sockfd, MSG_REQ_OL_USERS, NULL); 
            break;
        case 'a':
            sendMsgToServer(sockfd, MSG_REQ_ALL_USERS, NULL); 
            printf("All users in the database as follow:\n");
            break;
        case 's':
            {
                msg_st msg;
                msg.msgType = MSG_SEARCH_SOMEONE;
                printf("please input the name: ");
                scanf("%s", msg.name);
                write(sockfd, &msg, sizeof(msg_st));
            }
            break;
        case 'u':
            {
                startTalkToOtherUsers();
                menu2();
            }
            break;
        case 'r':
            {
                msg_st msg;
                msg.msgType = MSG_USER_RENAME;
                printf("please input your new name. \n");
                printf(">>: ");
                memset(msg.name, '\0', sizeof(char) * 24);
                scanf("%s", msg.name);
                write(sockfd, &msg, sizeof(msg_st));
            }
            break;
        case 'c':
            {
                msg_st msg;
                msg.msgType = MSG_USER_CHG_PW;
                printf("please input your new passward. \n");
                printf(">>: ");
                char oldInput[256] = {'\0'};
                scanf("%s", oldInput);
                printf("please input your passward again: \n");
                printf(">>: ");
                memset(msg.msgText, '\0', sizeof(char) * MAXSIZE);
                scanf("%s", msg.msgText);
                if(strcmp(oldInput, msg.msgText) == 0){
                    write(sockfd, &msg, sizeof(msg_st));
                }
                else{
                    printf("!!!! twice input is not the same.\n");
                }
            }
            break;
        case 'd':
            sendMsgToServer(sockfd, MSG_USER_UNREGIST, NULL); 
            break;
        case 'o':
            sendMsgToServer(sockfd, MSG_USER_LOGOUT, NULL); 
            break;
        case 'p':
            sendMsgToServer(sockfd, MSG_USER_REQ_MEDIA, NULL); 
            break;
        case 't':
            {
                sendMsgToServer(sockfd, MSG_USER_STOP_PLAY_MEDIA, NULL); 
                if(rtspClientPid != -1){
                    closeChildProcess(&rtspClientPid);
                    printf("******** rtsp client be closed ******** \n");
                }
                else{
                    printf("!!!!!! you have not open rtsp client to play media.\n");
                }
            }
            break;
        case 'q':
            menu2();
            printf(">>: ");
            break;
        default:
            printf("Command is not in the list, please input the right command!");
            break;
    }

    return c;
}

void closeChildProcess(int *pPid)
{
    if(*pPid != -1){
        kill(*pPid, SIGKILL);
        waitpid(*pPid, NULL, 0); /* 一定要wait 不然会产生僵尸线程 */
        *pPid = -1;
    }
    return;
}

void handleMessage(msg_st *msg)
{
    if(!msg) return;
    switch(msg->msgType){
        case MSG_FLASH_OL_USERS:
            {
                printf("    User-> name: %s; ip: %s\n", msg->name, msg->userIp);
                if(findNodeByName(onlineUser, msg->name) == NULL){ 
                    UIfo user;
                    strncpy(user.userId, msg->name, strlen(msg->name) +1);
                    strncpy(user.ip, msg->userIp, strlen(msg->userIp) + 1);
                    user.logFlag = LOGIN_YES;
                    addUserToList(onlineUser, user);
                }
            }
            break;
        case MSG_USER_RENAME_OK:
            {
                printf("change name ok, my new name is: %s\n", msg->name);
                UL *my = findNodeByName(onlineUser, msg->name);
                if(my != NULL){
                    strncpy(my->nodeInfo.userId, msg->name, strlen(msg->name) +1);
                }
                strncpy(myname, msg->name, strlen(msg->name) +1);
                menu2();
            }
            break;
        case MSG_USER_UNREGIST_OK:
            printf("dear, you are unregist your count!!!\n");
            usleep(500000);
            s_unRegistFlag = UN_REGIST_YES;
            break;
        case MSG_USER_LOGOUT_OK:
            s_loginFlag = LOGIN_NO;
            break;
        case MSG_BROCAST:
        case MSG_FLASH_ALL_USERS:
            printf("%s\n", msg->msgText);
            break;
        default:
            break;
    }
    return;
}

void talkingWithServer(int TcpSocket)
{
    fflush(stdin); //清缓存
    
    msg_st msg;
    menu2();
    printf("List of online users as following:\n");
    while(1)
    {
        fd_set readfd;
        FD_ZERO(&readfd);
        FD_SET(TcpSocket, &readfd);
        FD_SET(0, &readfd);  //把标准输入描述符加入描述符集
        int maxfd = ((0 > TcpSocket) ? 0 : TcpSocket);
        fd_set tmpfd = readfd;

        /* 发送数据 */
        int r = select(maxfd + 1, &readfd, NULL, NULL, NULL);
        if(r < 0){
            printf("select error!\n");
            closeChildProcess(&rtspClientPid);
            printf("******** rtsp client be closed ******** \n");
            exit(0);
        }
        else if(r == 0){
            printf("Waiting server reply!\n");
            continue;
        }
        else{
            //客户端输入数据
            if(FD_ISSET(0, &readfd)){
                memset(msg.msgText, '\0', sizeof(msg.msgText));
                fgets(msg.msgText, MAXSIZE, stdin);
                if(strncmp(msg.msgText, "quit", 4) == 0){
                    printf("are you sure to quit: yes or no \n");
                    char buffer[5];
                    memset(buffer, 0, sizeof(buffer));
                    fgets(buffer, MAXSIZE, stdin);
                    if(strncmp(buffer, "yes", 3)== 0){
                        sendMsgToServer(TcpSocket, MSG_USER_QUIT, NULL); 
                        sleep(2);
                        FD_CLR(TcpSocket, &readfd);
                        close(TcpSocket);
                        closeChildProcess(&rtspClientPid);
                        printf("******** rtsp client be closed ******** \n");
                        exit(0);
                    }
                    else
                        continue;
                }

                if(strncmp(msg.msgText, "cmd", 3) == 0){
                    menu3();
                    getCommand3(TcpSocket);
                    continue;
                }

                int len = strlen(msg.msgText);
                msg.msgText[len - 1] = '\0';	
                sendMsgToServer(TcpSocket, MSG_BROCAST, msg.msgText); 
            }
            //接收从服务器发来的数据
            if(FD_ISSET(TcpSocket, &readfd)){
                int nbytes;
                memset(msg.msgText, '\0', MAXSIZE * sizeof(char));
                if((nbytes = read(TcpSocket, &msg, sizeof(msg_st))) == -1){ 
                    fprintf(stderr,"Read Error:%s\n",strerror(errno));
                    closeChildProcess(&rtspClientPid);
                    printf("******** rtsp client be closed ******** \n");
                    exit(1); 
                } 
                if(nbytes == 0){
                    printf("Tcp server has exit, connect over!!!!\n");
                    close(TcpSocket);
                    closeChildProcess(&rtspClientPid);
                    printf("******** rtsp client be closed ******** \n");
                    exit(0);
                }

                handleMessage(&msg);
                if(msg.msgType == MSG_SERVER_MEDIA_PLAYING){
                    char port[20] = {'\0'};
                    int len = strlen(msg.msgText);
                    strncpy(port, msg.msgText, len);
                    port[len] = '\0';
                    if(rtspClientPid == -1){
                        rtspClientPid = startPlayShareMedia(port);
                        printf("+++++++++startPlayShare media child pid = %d\n", rtspClientPid);
                    }
                }

                if(s_unRegistFlag == UN_REGIST_YES){
                    s_unRegistFlag = UN_REGIST_NO;
                    s_loginFlag = LOGIN_NO;
                    closeChildProcess(&rtspClientPid);
                    printf("******** rtsp client be closed ******** \n");
                    return;
                }

                if(s_loginFlag == LOGIN_NO){
                    printf("you have logout!!!!\n");
                    closeChildProcess(&rtspClientPid);
                    printf("******** rtsp client be closed ******** \n");
                    return;
                }
            }
        }
    }
    
    closeChildProcess(&rtspClientPid);
    printf("******** rtsp client be closed ******** \n");
    return;
}

int main(int argc, char *argv[]) 
{ 
    int Tcpsockfd; 
    char buffer[MAXSIZE]; 
    struct sockaddr_in server_addr; 
    struct hostent *host; 

    system("clear");

    if(argc != 1 && argc != 2){
        printf("Useage: ./user [ip_address]\n");
        printf("Detail:\n");
        printf("    <1> ./user\n");
        printf("    <2> ./user xx.xxx.xx.xx\n");
        exit(-1);
    }
    
    signal(SIGPIPE,SIG_IGN);

    /* 如果没有参数指定服务器ip，则读取文件中的ip地址 */
    if(argc == 1){
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
        printf("ip = %s\n", ip);

        memset(server_ip, '\0', sizeof(char) * 30);
        strcpy(server_ip, ip);
    }
    else{
        memset(server_ip, '\0', sizeof(char) * 30);
        strcpy(server_ip, argv[1]);
    }

    /* 客户程序开始建立 sockfd描述符 */ 
    if((Tcpsockfd=socket(AF_INET,SOCK_STREAM,0))==-1){ // AF_INET:Internet;SOCK_STREAM:TCP 
        fprintf(stderr,"Socket Error:%s\a\n",strerror(errno)); 
        exit(1); 
    } 

    /* 客户程序填充服务端的资料 */ 
    bzero(&server_addr,sizeof(server_addr)); // 初始化,置0
    server_addr.sin_family=AF_INET;          // IPV4
    server_addr.sin_port=htons(portnumber);  // (将本机器上的short数据转化为网络上的short数据)端口号
    //server_addr.sin_addr=*((struct in_addr *)host->h_addr); // IP地址
    struct in_addr ipaddr;
    inet_aton(server_ip, &ipaddr);
    server_addr.sin_addr = ipaddr;
    /* 客户程序发起连接请求 */ 
    if(connect(Tcpsockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1){ 
        fprintf(stderr,"Connect Error:%s\a\n",strerror(errno)); 
        exit(1); 
    } 

    printf("Success to connect to Tcp server...\n");
    pthread_t pwork;

    onlineUser = initUserList();
    while(1)
    {
        menu1(); //命令
        getCommand1(Tcpsockfd);
        //printf("s_loginFlag = %d\n", s_loginFlag);
        if(s_loginFlag == LOGIN_YES){
             /*向服务器索要在线用户的名单*/
            sendMsgToServer(Tcpsockfd, MSG_REQ_OL_USERS, NULL); 
            talkingWithServer(Tcpsockfd);
        }
    }
    /* 结束通讯 */ 
    close(Tcpsockfd); 
    freeUserList(onlineUser);
    exit(0); 
} 
