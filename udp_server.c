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
  
struct node   
{  
    char name[20];  
    struct sockaddr_in client_addr;  
    struct node *next;  
};  
  
struct message  
{  
    long type;  
    char name[20];  
    char peer_name[20];  
    char mtext[512];  
};  
  
struct node *create_list(void);  
void insert_list(struct node *, char *, struct sockaddr_in *);  
void delete_list(struct node *, char *);  
  
void recv_message(int , struct node *);  
void send_message(int , struct sockaddr_in *, pid_t );  
  
void client_login(int , struct node *, struct message *, struct sockaddr_in *);  
void client_chat(int , struct node *, struct message *);  
void client_quit(int , struct node *, struct message *);  
void server_chat(int , struct node *, struct message *);  
void server_quit(int , struct node *, struct message *);  
  
void brocast_msg(int , struct node *, struct message *);  
void print_online(int , struct node *, struct message *);  
void private_chat(int , struct node *, struct message *);  
  
void father_func(int sig_no)  
{  
    return ;  
}  
  
int main(int argc, const char *argv[])  
{  
    int socket_fd;  
    pid_t pid;  
    struct sockaddr_in server_addr;  
    struct node *head;  
  
    if (argc < 3){  
        fprintf(stderr, "usages : %s ip port\n", argv[0]);  
        exit(-1);  
    }  
  
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){  
        perror("failed to create socket");  
        exit(-1);  
    }  
  
    head = create_list();  
  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(atoi(argv[2]));  
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);  
  
    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){  
        perror("failed to bind");  
        exit(-1);  
    }  
  
    if ((pid = fork()) < 0){  
        perror("failed to fork pid");  
        exit(-1);  
    }  

    
    if (pid == 0){  
        printf("udp server <receive message> running......\n");
        recv_message(socket_fd, head);  
    }
    else{  
        printf("udp server <send message> running......\n");
        send_message(socket_fd, &server_addr, pid); 
    }
  
    return 0;  
}  
  
  
struct node *create_list(void)  
{  
    struct node *head;  
  
    head = (struct node *)malloc(sizeof(struct node));  
    head->next = NULL;  
  
    return head;  
}  
  
void insert_list(struct node *head, char *name, struct sockaddr_in *client_addr)  
{  
    if(!head) return;
    struct node *new;  
  
    new = (struct node *)malloc(sizeof(struct node));  
    strcpy(new->name, name);  
    new->client_addr = *client_addr;  
  
    new->next = head->next;  
    head->next = new;  
  
    return ;  
}  
  
void delete_list(struct node *head, char *name)  
{  
    if(!head) return;
    struct node *p = head->next;  
    struct node *q = head;  
  
    while (p != NULL){  
        if (strcmp(p->name, name) == 0)  
            break;  
  
        p = p->next;  
        q = q->next;  
    }  
  
    q->next = p->next;  
    p->next = NULL;  
    free(p);  
  
    return ;  
}  
  
void recv_message(int socket_fd, struct node *head)  
{  
    struct message msg;  
    struct sockaddr_in client_addr;  
    int client_addrlen = sizeof(struct sockaddr);  
  
    while (1)  
    {  
        memset(&msg, 0, sizeof(msg));  
        if (recvfrom(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_addr, &client_addrlen) < 0){  
            perror("failed to recvform client");  
            exit(-1);  
        }  
  
        switch(msg.type)  
        {  
            case CLIENT_LOGIN:  
                client_login(socket_fd, head, &msg, &client_addr);  
                break;  
  
            case CLIENT_CHAT:  
                client_chat(socket_fd, head, &msg);  
                break;  
  
            case CLIENT_QUIT:  
                client_quit(socket_fd, head, &msg);  
                break;  
  
            case SERVER_CHAT:  
                server_chat(socket_fd, head, &msg);  
                break;  
  
            case SERVER_QUIT:  
                server_quit(socket_fd, head, &msg);  
                break;  
  
            case PRINT_ONLINE:  
                print_online(socket_fd, head, &msg);  
                break;  
  
            case PRIVATE_CHAT:  
                private_chat(socket_fd, head, &msg);  
                break;  
  
            default:  
                break;  
        }  
    }  
  
    return ;  
}  
  
void send_message(int socket_fd, struct sockaddr_in *server_addr, pid_t pid)  
{  
    struct message msg;  
    char buf[512];  
  
    signal(getppid(), father_func);  
      
    while (1)  
    {  
        usleep(500);  
        printf(">");  
        fgets(buf, sizeof(buf), stdin);  
        buf[strlen(buf) - 1] = 0;  
  
        strcpy(msg.mtext, buf);  
        strcpy(msg.name , "server");  
        msg.type = SERVER_CHAT;  
  
        if (strncmp(buf, "quit", 4) == 0){  
            msg.type = SERVER_QUIT;  

            printf("udp server will quit, socket_fd = %d!!!!!\n", socket_fd);
            if (sendto(socket_fd, &msg, sizeof(msg), 0,   
                        (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
                perror("failed to send server_quit message");  
                exit(-1);  
            }  
  
            //pause();  
  
            kill(pid, SIGKILL);  
            waitpid(pid, NULL, 0);  
            close(socket_fd);  
            exit(-1);  
        }  
  
        if (sendto(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) < 0){  
            perror("failed to send server_chat message");  
            exit(-1);  
        }  
    }  
  
    return ;  
}  
  
void client_login(int socket_fd, struct node *head, struct message *msg, struct sockaddr_in *client_addr)  
{  
    printf("********Login In Notice********\n");  
    printf("%s has Login In, you can talk to him/her.\n", msg->name);  
    printf("****************************\n");  
  
    insert_list(head, msg->name, client_addr);  
    brocast_msg(socket_fd, head, msg);  
  
    return ;  
}  
  
void client_chat(int socket_fd, struct node *head, struct message *msg)  
{  
    printf("********Group Chat********\n");  
    printf("name: %s\n", msg->name);  
    printf("msg: %s\n", msg->mtext);  
    printf("**************************\n");  
  
    brocast_msg(socket_fd, head, msg);  
  
    return ;  
}  
  
void client_quit(int socket_fd, struct node *head, struct message *msg)  
{  
    printf("*********Quit Msg********\n");  
    printf("%s is Quit\n", msg->name);  
    printf("*************************\n");  
  
    delete_list(head, msg->name);  
    brocast_msg(socket_fd, head, msg);  
  
    return ;  
}  
  
void server_chat(int socket_fd, struct node *head, struct message *msg)  
{  
    printf("********Server Msg*********\n");  
    printf("msg: %s\n", msg->mtext);  
    printf("***************************\n");  
  
    brocast_msg(socket_fd, head, msg);  
  
    return ;  
}  
  
void server_quit(int socket_fd, struct node *head, struct message *msg)  
{  
    brocast_msg(socket_fd, head, msg);  
    kill(getppid(), SIGUSR1);  
  
    return ;  
}  
  
void print_online(int socket_fd, struct node *head, struct message *msg)  
{  
    struct node *p = head->next;  
    struct sockaddr_in my_addr;  
    char buf[512];  
  
    printf("%s is request to print online client\n", msg->name);  
    memset(buf, 0, sizeof(buf));  
  
    while (p != NULL)  
    {  
        if (strcmp(p->name, msg->name) == 0){  
            my_addr = p->client_addr;  
            p = p->next;
            continue;
        }
  
        strcat(buf, p->name);  
        strcat(buf, " ");  
          
        p = p->next;  
    }  
  
    strcpy(msg->mtext, buf);  
    msg->type = PRINT_ONLINE;  
  
    if (sendto(socket_fd, msg, sizeof(struct message), 0,   
                (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0){  
        perror("failed to send print_online message");  
        exit(-1);  
    }  
  
    return ;  
}  
  
void private_chat(int socket_fd, struct node *head, struct message *msg)  
{  
    struct node *p = head->next;  
    struct sockaddr_in *peer_addr = NULL;  
  
    printf("******** Private Msg ********\n");  
    printf("from %s\n", msg->name);  
    printf("to %s\n", msg->peer_name);  
    printf("msg: %s\n", msg->mtext);  
    printf("*****************************\n");  
  
    while (p != NULL)  
    {  
        if (strcmp(p->name, msg->peer_name) == 0){  
            peer_addr = &(p->client_addr);  
            break;  
        }  
  
        p = p->next;  
    }  

    if(!peer_addr){
        printf("[Udp server] user %s is not in online list!!\n", msg->peer_name);
        msg->type = SERVER_CHAT;
        sprintf(msg->mtext, "The one of %s who you want to talk is not exist!!!\n", msg->peer_name);
        p = head->next;
        while (p != NULL){  
            if (strcmp(p->name, msg->name) == 0){  
                peer_addr = &(p->client_addr);  
                break;  
            }  
            p = p->next;  
        }
    }

    if(peer_addr){
        if (sendto(socket_fd, msg, sizeof(struct message), 0,   
                    (struct sockaddr *)peer_addr, sizeof(struct sockaddr_in)) < 0){  
            perror("failed to send private message");  
            exit(-1);  
        }  
    }
  
    return ;  
}  
  
void brocast_msg(int socket_fd, struct node *head, struct message *msg)  
{  
    struct node *p = head->next;  
  
    while(p != NULL)  
    {  
        if (msg->type == CLIENT_LOGIN){  
            if (strcmp(p->name, msg->name) == 0){  
                p = p->next;  
                continue;  
            }  
        }  
  
        sendto(socket_fd, msg, sizeof(struct message), 0, (struct sockaddr *)&(p->client_addr), sizeof(struct sockaddr));  
        p = p->next;  
    }  
  
    return ;  
}  