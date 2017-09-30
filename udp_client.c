#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <signal.h>  
  
#define CLIENT_LOGIN    100  
#define CLIENT_CHAT     200  
#define CLIENT_QUIT     300  
  
#define SERVER_CHAT     400  
#define SERVER_QUIT     500  
  
#define PRINT_ONLINE    600  
#define PRIVATE_CHAT    700  

static char s_my_name[20];

struct message  
{  
    long type;  
    char name[20];  
    char peer_name[20];  
    char mtext[512];  
};  
  
void recv_message(int );  
void send_message(int , struct sockaddr_in *, char *, pid_t);  
  
void login_msg(struct message *);  
void group_msg(struct message *);  
void quit_msg(struct message *);  
void server_msg(struct message *);  
void server_quit(void);  
void online_msg(struct message *);  
void private_msg(struct message *);  
  
void print_online(int , struct message *, struct sockaddr_in *);  
void group_chat(int , struct message *, struct sockaddr_in *);  
void private_chat(int ,struct message *, struct sockaddr_in *);  
void client_quit(int , struct message *, struct sockaddr_in *, pid_t );  
  
int main(int argc, char *argv[])  
{  
    pid_t pid;  
    int server_fd;  
    struct sockaddr_in server_addr;  
  
    if (argc < 4){  
        fprintf(stderr, "usages: %s ip port name\n", argv[0]);  
        exit(-1);  
    }  
  
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){  
        perror("failed to create server_fd");  
        exit(-1);  
    }  
  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(atoi(argv[2]));  
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);  
  
    if ((pid = fork()) < 0){  
        perror("failed to fork pid");  
        exit(-1);  
    }  

    memset(s_my_name, '\0', 20);
    memcpy(s_my_name, argv[3], sizeof(char) * (strlen(argv[3]) + 1));
  
    if (pid == 0)  
        recv_message(server_fd);  
    else  
        send_message(server_fd, &server_addr, argv[3], pid);  
  
    return 0;  
}  
  
void recv_message(int server_fd)  
{  
    struct message msg;  
  
    while (1)  
    {  
        memset(&msg, 0, sizeof(msg));  

        int nbytes = 0;
        if ((nbytes = recvfrom(server_fd, &msg, sizeof(msg), 0, NULL, NULL)) < 0){  
            perror("failed to recv server message");  
            exit(-1);  
        }  
  
        switch(msg.type)  
        {  
            case CLIENT_LOGIN:  
                login_msg(&msg);  
                break;  
  
            case CLIENT_CHAT:  
                group_msg(&msg);  
                break;  
  
            case CLIENT_QUIT:  
                quit_msg(&msg);  
                break;  
  
            case SERVER_CHAT:  
                server_msg(&msg);  
                break;  
  
            case SERVER_QUIT:  
                server_quit();  
                break;  
  
            case PRINT_ONLINE:  
                online_msg(&msg);  
                break;  
  
            case PRIVATE_CHAT:  
                private_msg(&msg);  
                break;  
            default:  
                break;  
        }  
    }  
  
    return ;  
}  
  
void send_message(int server_fd, struct sockaddr_in *server_addr, char *name, pid_t pid)  
{  
    struct message msg;  
    char buf[512];  
    int c;  
  
    msg.type = CLIENT_LOGIN;  
    strcpy(msg.name, name);  
  
    if (sendto(server_fd, &msg, sizeof(msg), 0,   
                (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
        perror("failed to send login message");  
        exit(-1);  
    }  
  
    while(1)  
    {  
        usleep(500);  
        printf("#####################################\n");  
        printf("the following options can be choose:\n");  
        printf("input 1 is to print online client\n");  
        printf("input 2 is to chat to all the client\n");  
        printf("input 3 is to chat to only one client\n");  
        printf("input q is to quit\n");  
        printf("#####################################\n");  
          
        printf(">");  
        c = getchar();  
        while (getchar() != '\n');   
  
        switch(c)  
        {  
            case '1':  
                print_online(server_fd, &msg, server_addr);  
                break;  
  
            case '2':
                printf("[Notice:]you can input <quit> cmd to close group chat....\n");
                group_chat(server_fd, &msg, server_addr);  
                break;  
  
            case '3':  
                printf("[Notice:]you can input <quit> cmd to close private chat....\n");
                private_chat(server_fd, &msg, server_addr);  
                break;  
  
            case 'q':  
                client_quit(server_fd, &msg, server_addr, pid);  
                break;  
  
            default:  
                break;  
        }  
    }  
  
    return ;  
}  
  
void login_msg(struct message *msg)  
{  
    printf("********Login In Notice********\n");  
    printf("%s has Login In, you can talk to him/her.\n", msg->name);  
    printf("****************************\n");    
  
    return ;  
}  
  
void group_msg(struct message *msg)  
{  
    printf("******** Group Msg ********\n");  
    printf("name: %s\n", msg->name);  
    printf("msg: %s\n", msg->mtext);  
    printf("******** Group Msg ********\n");  
  
    return ;  
}  
  
void quit_msg(struct message *msg)  
{  
    printf("######## Quit Msg ########\n");  
    printf("%s is Quit\n", msg->name);  
    printf("######## Quit Msg ########\n");  
  
    return ;  
}  
  
void server_msg(struct message *msg)  
{  
    printf("******** Server Msg ********\n");  
    printf("msg: %s\n", msg->mtext);  
    printf("******** Server Msg ********\n");  
  
    return ;  
}  
  
void server_quit(void )  
{  
    kill(getppid(), SIGKILL);  
    exit(0);  
}  
  
void online_msg(struct message *msg)  
{  
    printf("******** Clients you can talk to. ********\n");  
    printf("Clients: %s\n", msg->mtext);  
    printf("******** Clients you can talk to. ********\n");  
  
    return ;  
}  
  
void private_msg(struct message *msg)  
{  
    printf("******** Private Msg ********\n");  
    printf("[%s] say: %s\n", msg->name, msg->mtext);  
    printf("******** Private Msg ********\n");  
  
    return ;  
}  
  
void print_online(int server_fd, struct message *msg, struct sockaddr_in *server_addr)  
{  
    msg->type = PRINT_ONLINE;  
  
    if (sendto(server_fd, msg, sizeof(struct message), 0,   
                (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
        perror("failed to send online message");  
        exit(-1);  
    }  
  
    return ;  
}  
  
void group_chat(int server_fd, struct message *msg, struct sockaddr_in *server_addr)  
{  
    char buf[512];  
  
    memset(buf, 0, sizeof(buf));  
    printf("*********** Group Chat Room ***********\n");  
    printf("****** Welcome to group chat room ******\n");  
    printf("* if you want to quit, please input quit\n");  
    printf("****************************************\n");  
  
    while(1)  
    {  
        memset(buf, 0, sizeof(buf));  
        usleep(500);  
        printf(">");  
        fgets(buf, sizeof(buf), stdin);  
        buf[strlen(buf) - 1] = 0;  
  
        msg->type = CLIENT_CHAT;  
        strcpy(msg->mtext, buf);  
  
        if (strncmp(buf, "quit", 4) == 0)  
            break;  
  
        if (sendto(server_fd, msg, sizeof(struct message), 0,   
                    (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
            perror("failed to send group message");  
            exit(-1);  
        }  
    }  
  
    return ;  
}  
  
void private_chat(int server_fd, struct message *msg, struct sockaddr_in *server_addr)  
{  
    char name[20];  
    char buf[512];  
  
    memset(name, 0, sizeof(name));  
    printf("please input the peer_name\n");  
    printf(">");  
    fgets(name, sizeof(name), stdin);  
    name[strlen(name) - 1] = 0;  

    if(strncmp(s_my_name, name, strlen(name)) == 0){
        printf("you can not talk with yourself!!!!!\n");
        return;
    }
    
    strcpy(msg->peer_name, name);  
    msg->type = PRIVATE_CHAT;  
  
    printf("you want to talk to %s!\n", msg->peer_name);  
  
    while(1)  
    {  
        usleep(500);  
        printf(">");  
        fgets(buf, sizeof(buf), stdin);  
        buf[strlen(buf) - 1] = 0;  
  
        strcpy(msg->mtext, buf);  
  
        if (strncmp(buf, "quit", 4) == 0)  
            break;  
  
        if (sendto(server_fd, msg, sizeof(struct message), 0,   
                    (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
            perror("failed to send private message");  
            exit(-1);  
        }  
    }  
  
    return ;  
}  
  
void client_quit(int server_fd, struct message *msg, struct sockaddr_in *server_addr, pid_t pid)  
{  
    msg->type = CLIENT_QUIT;  
  
    if (sendto(server_fd, msg, sizeof(struct message), 0,   
                (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
        perror("failed to send quit message");  
        exit(-1);  
    }  
  
    kill(pid, SIGKILL);  
    waitpid(pid, NULL, 0);  
    exit(0);  
}  