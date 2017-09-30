#ifndef __USER_LIST_H__
#define __USER_LIST_H__

#include <stdlib.h> 
#include <stdio.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>

typedef struct UserInfo
{
	char userId[1024];  //user name
	int socketfd;    //user socket fd
	int logFlag;     //user login state
	int rtspPlayFlag;
	char ip[30];     //user ip addr
	sem_t sem;
}UIfo;

typedef struct UserList
{
	UIfo nodeInfo;
	struct UserList* next;
}UL;

UL* initUserList();

void addUserToList(UL* head, UIfo userInfo);

void freeUserList(UL* head);

UL* findNodeByName(UL* head, const char* name);

int numberOfList(UL* head);

int numberOfOnlineUsers(UL* head);
	
UL* findNodeBySocketID(UL* head, const int sid);

void deleteOneNode(UL* head, UL* dNode);

void printAllUserInfo(UL* head);

void showAllUsers(UL* head);

void deleteNodeByName(UL* head, const char* name);

void setLoginByName(UL* head, const char* name, const int islogin);

#endif
