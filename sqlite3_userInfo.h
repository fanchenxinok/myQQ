#ifndef __SQLITE3_USER_INFO__
#define __SQLITE3_USER_INFO__

#include "userList.h"

#define SQLITE_WAL_MODE (1)
#define NEED_ENCRYPT (1)  /* 是否需要加密 */
#define NEED_SAVE_LOGIN_FLAG (0)

typedef enum{
    LOGIN_NO = 0,
    LOGIN_YES = 1
}EM_LOGIN_F;

typedef struct __acount_info__
{
    char name[24];
    char passward[24];
    EM_LOGIN_F islogin;
}acount_info_st;

#define QQ_DB_NAME "qqUserInfo.db"   /**< database name */
#define QQ_TABLE_NAME "qqUserInfoTab"

typedef enum
{
    QQ_FAIL = -1,
    QQ_OK,
    QQ_USER_LOGIN,
    QQ_USER_UNLOGIN,
    QQ_USER_EXIST,
    QQ_USER_UN_EXIST,
    QQ_PARAM_INVALID,
    QQ_PASSWARD_WRONG
}EM_RES;

EM_RES myQQ_Init();

void myQQ_DeInit();

EM_RES myQQ_LoginCheck(const char* name, const char* passward);

EM_RES myQQ_Register(const char* name, const char* passward);

EM_RES myQQ_UnRegister(const char* name);

EM_RES myQQ_ChangeName(const char* oldName, const char* newName);

EM_RES myQQ_ChangePassward(const char* name, const char* newPassward);
#if NEED_SAVE_LOGIN_FLAG
EM_RES myQQ_SetUserLogin(const char* name);

EM_RES myQQ_SetUserLogout(const char* name);

EM_RES myQQ_UserIsLogin(const char* name);
#endif
EM_RES myQQ_GetAllUsers(UL* userHead);

EM_RES myQQ_IsSamePassward(const char* name, const char* passward);

#endif
