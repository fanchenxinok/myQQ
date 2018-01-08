#include "userList.h"

UL* initUserList()
{
	UL* new = (UL*)malloc(sizeof(UL));
	if(!new){
		printf("malloc error!\n");
		exit(0);
	}
	memset(&(new->nodeInfo), 0, sizeof(UIfo));
	new->next = NULL;
	return new;
}

void addUserToList(UL* head, UIfo userInfo)
{
	if(!head){
	    printf("head is NULL!\n");
	    return;
	}
	UL* new = (UL*)malloc(sizeof(UL));
	if(!new){
		printf("malloc error!\n");
		exit(0);
	}
	memcpy(&new->nodeInfo, &userInfo, sizeof(UIfo));
	new->next = head->next;
	head->next = new;
       return;
}

void addUserToListTail(UL* tail, UIfo userInfo)
{
       if(!tail){
            printf("tail is NULL!\n");
            return;
       }
	UL* new = (UL*)malloc(sizeof(UL));
	if(!new){
		printf("malloc error!\n");
		exit(0);
	}
	memcpy(&new->nodeInfo, &userInfo, sizeof(UIfo));
       tail->next = new;
	new->next = NULL;
       return;
}


void freeUserList(UL* head)
{
	UL* p = head;
	while(p != NULL){
		head = head->next;
		free(p);
		p = head;
	}
}
UL* findNodeByName(UL* head, const char* name)
{
    if(!head || !name) return NULL;
    UL* p = head->next;
    while(p != NULL){
        if(strncmp(p->nodeInfo.userId, name, strlen(name)) != 0){
            p = p->next;
        }
        else
            return p;
    }
    return NULL;
}

int numberOfList(UL* head)
{
	if(!head) return -1;
	UL* p = head ->next;
	int n = 0;
	while(p != NULL){
		n++;
		p = p->next;
	}
	return n;
}

int numberOfOnlineUsers(UL* head)
{
	if(!head) return -1;
	UL* p = head ->next;
	int n = 0;
	while(p != NULL){
              if(p->nodeInfo.logFlag == 1)
		    n++;
		p = p->next;
	}
	return n;
}

	
UL* findNodeBySocketID(UL* head, const int sid)
{
    if(!head) return NULL;
    UL* p = head->next;
    while(p != NULL){
        if(p->nodeInfo.socketfd != sid)
        	p = p->next;
        else
        	break;
    }
    return p;
}

void deleteOneNode(UL* head, UL* dNode)
{
    if(!head || !dNode) return;
    UL* p = head->next;
    UL* prev = head;
    while(p != NULL){
        if(p == dNode) break;
        prev = p;
        p = p->next;
    }
    if(p != NULL){
        prev ->next = p->next;
        free(p);
        p = NULL;
    }
    return;
}

void printAllUserInfo(UL* head)
{
    if(!head) return;
    UL *p = head->next;
    while(p){
        printf("name:%s, sockfd:%d, login_flag:%s\n", 
            p->nodeInfo.userId, p->nodeInfo.socketfd,
            (p->nodeInfo.logFlag == 1) ? "LOGIN_YES" : "LOGIN_NO");
        p = p->next;
    }
    return;
}

void showAllUsers(UL* head)
{
    if(!head) return;
    UL *p = head->next;
    int i = 1;
    while(p){
        printf("    <%d>name:%s, login_flag:%s\n", i, p->nodeInfo.userId,
            (p->nodeInfo.logFlag == 1) ? "LOGIN_YES" : "LOGIN_NO");
        p = p->next;
        i++;
    }
    return;
}

void deleteNodeByName(UL* head, const char* name)
{
    if(!head || !name) return;

    UL* p = head->next;
    UL* prev = head;
    while(p != NULL){
        if(strncmp(p->nodeInfo.userId, name, strlen(name)) == 0) break;
        prev = p;
        p = p->next;
    }
    
    if(p != NULL){
        //printf("deleteNode: name = %s, ip =%s\n", p->nodeInfo.userId, p->nodeInfo.ip);
        prev ->next = p->next;
        free(p);
        p = NULL;
    }
    return;
}

void setLoginByName(UL* head, const char* name, const int islogin)
{
    if(!head || !name) return;
    UL* p = head->next;
    while(p != NULL){
        if(strncmp(p->nodeInfo.userId, name, strlen(name)) != 0){
            p = p->next;
        }
        else
            break;
    }

    if(p != NULL){
        p->nodeInfo.logFlag = islogin;
    }
    return;
}

void setLoginBySockID(UL* head, const int sockfd, const int islogin)
{
    if(!head) return;
    UL* p = head->next;
    while(p != NULL){
        if(p->nodeInfo.socketfd != sockfd){
            p = p->next;
        }
        else
            break;
    }

    if(p != NULL){
        p->nodeInfo.logFlag = islogin;
    }
    return;
}

