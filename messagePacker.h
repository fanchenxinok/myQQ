#ifndef __QQ_MSG_PACKER__
#define __QQ_MSG_PACKER__

typedef enum
{
    MSG_REQ_OL_USERS = 0, /* user request show online users */ //u
    MSG_FLASH_OL_USERS,   /* flashing online users *///s
    MSG_REQ_ALL_USERS, /* user request show all users in database*///u
    MSG_FLASH_ALL_USERS,   /* flashing all users in database *///s
    MSG_SEARCH_SOMEONE,//u
    MSG_USER_LOGIN_CHECK,//u
    MSG_USER_LOGIN_OK,//s
    MSG_USER_LOGIN_FAIL,//s
    MSG_USER_LOGOUT,//u
    MSG_USER_LOGOUT_OK,//s
    MSG_USER_REGIST,//u
    MSG_USER_REGIST_OK,//s
    MSG_USER_UNREGIST,//u
    MSG_USER_UNREGIST_OK,//s
    MSG_USER_RENAME,//u
    MSG_USER_RENAME_OK,//s
    MSG_USER_CHG_PW,//u
    MSG_USER_EXIST, //s
    MSG_USER_UN_EXIST, //s
    MSG_USER_HAS_ONLINE,//s
    MSG_USER_QUIT,//u
    
    MSG_USER_REQ_MEDIA,//u
    MSG_SERVER_MEDIA_PLAYING, //s
    MSG_USER_STOP_PLAY_MEDIA, //u
    
    MSG_FAIL,
    MSG_OK,
    MSG_BROCAST,//s
    MSG_END
}emMsgType;

typedef struct _message_
{
    emMsgType msgType;
    char name[20];
    char msgText[512];
}msg_st;

/**
* @brief function: 消息打包器
* @param[in] msgData: 用来存储打包的消息
* @return void.
*/
void apl_msgPacker(char* msgData, ...);

/**
* @brief function: 消息解包器
* @param[in] msgData: 用来存储打包的消息
* @return void.
*/
void apl_msgUnPacker(char* msgData, ...);

/*example*/
/*
(1) pack message
char msgData[128];
int a = 10, b = 100;
apl_msgPacker(msgData,
		 	sizeof(int), &a,
		 	sizeof(int), &b, -1);
(2) unpack message
int a, b;
apl_msgUnPacker(msgData,
		 	sizeof(int), &a,
		 	sizeof(int), &b, -1);
cout << a << b << endl;
*/
#endif
