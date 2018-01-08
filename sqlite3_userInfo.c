#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "sqlite3_userInfo.h"

static sqlite3* qqUserInfoDb = NULL;
static pthread_mutex_t s_sidb_write_lock;
#define ENCRYPT_FACTOR (0x12) /* 静态加密因子*/

#define qq_check_return(err_cond, ret) \
    do{\
        if(err_cond){ \
            printf("[qq db] check '%s' failed at:%u\n", #err_cond, __LINE__);\
            return ret;\
        }\
    }while(0)

/* 随机产生加密因子 */
static unsigned char qq_encrypt_factor_get()
{
    srand(time(NULL));
    unsigned char ef = rand() % 0xff;
    return ef;
}

/* 用动态加密因子加密 */
static int qq_encrypt(const char* str, unsigned char* uInt8, unsigned char encrypt_factor)  
{  
    if(!str || !uInt8) return -1;
    int nBytes = strlen(str) + 1;
    int i = 0;
    for(; i < nBytes; i++){
        uInt8[i] = (unsigned char)str[i];
        if((uInt8[i] + encrypt_factor) > 0xff){
            uInt8[i] = uInt8[i] + encrypt_factor - 0xff; 
        }
        else{
            uInt8[i] = uInt8[i] + encrypt_factor;
        }
    }
    return i;
}  

static void qq_un_encrypt(
    char* str,
    const unsigned char* uInt8,
    int n,
    unsigned char encrypt_factor)  
{  
    if(!str || !uInt8) return;
    int i = 0;  
    unsigned char cv = 0x00;
    for(i = 0; i < n; i++){
        cv = uInt8[i];
        if((cv - encrypt_factor) < 0x00){
            cv = uInt8[i] + 0xff - encrypt_factor;
        }
        else{
            cv = uInt8[i] - encrypt_factor;
        }
        str[i] = (char)cv;  
    }  
    return;
}  

#if NEED_SAVE_LOGIN_FLAG
static int qq_sql3_create_table()
{
    char sql[512];
    const char* tableName = QQ_TABLE_NAME;
    #if NEED_ENCRYPT
    sprintf(sql, "create table if not exists %s (" \
        "name text not null," \
        "passward blob not null," \
        "islogin int1 not null);", tableName);
    #else
    sprintf(sql, "create table if not exists %s (" \
        "name text not null," \
        "passward text not null" \
        "islogin int1 not null);", tableName);
    #endif

    printf("[QQ DB] %s\n", sql);

    /* 创建表 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            return -1;
        }else{
            printf("[QQ DB]Table %s created successfully\n", tableName);
        }
    }
    return 0;
}

static int qq_sql3_init()
{
    char* zErrMsg;
    int rc = 0;
    /* 数据库创建或打开 */
    rc = sqlite3_open(QQ_DB_NAME, &qqUserInfoDb);

    if( rc ){
       printf("[QQ DB]Can't open database: %s\n", sqlite3_errmsg(qqUserInfoDb));
       return -1;
    }else{
       printf("[QQ DB]Opened database successfully\n");
    }

#if SQLITE_WAL_MODE
    rc = sqlite3_exec(qqUserInfoDb, "PRAGMA journal_mode=WAL;", NULL, NULL, &zErrMsg);
    if( rc != SQLITE_OK ){
        printf("[QQ DB]SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    /* 指定提交的大小，默认为1KB */
    //rc = sqlite3_exec(db, "PRAGMA wal_autocheckpoint=100;", NULL, 0, &zErrMsg);
#endif  //#if SQLITE_WAL_MODE

    pthread_mutex_init(&s_sidb_write_lock, NULL);

    pthread_mutex_lock(&s_sidb_write_lock);
    rc  = qq_sql3_create_table();
    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_insert(const char* name, const char* passward, const EM_LOGIN_F islogin)
{
    int rc = 0;
    if(!name || !passward){
        printf("[QQ DB]qq_sql3_insert() name or passward is NULL!!!\n");
        return -1;
    }
    
    pthread_mutex_lock(&s_sidb_write_lock);

    char sql[512];
    sprintf(sql, "insert into %s (name,passward,islogin) " \
                            "values (:name,:passward,:islogin);", QQ_TABLE_NAME);

    printf("[QQ DB]%s\n", sql);
                 
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL); /* 准备语句对象 */
    if(rc != SQLITE_OK){
        if(qqUserInfoDb == NULL){
            printf("[QQ DB]qqUserInfoDb == NULL!!!!\n");
        }
        printf("[QQ DB]prepare Insert sql stmt Fail!!!!!!!!, rc = %d\n", rc);
         sqlite3_finalize(stmt);
        return -1;
    }
    
    if(qqUserInfoDb != NULL){
       sqlite3_reset(stmt);
      
       /* 将需要插入的数据 绑定到stmt对象 */
       #if  NEED_ENCRYPT
       sqlite3_bind_text(stmt, 1, name, -1, NULL);
       unsigned char ef = qq_encrypt_factor_get();
       unsigned char uInt8_pw[1024];
       int n = qq_encrypt(passward, uInt8_pw, ef);
       uInt8_pw[n] = ef;  /* 动态加密因子存储在密码的最后一个字节 */
       printf("[QQ DB]encrypt_factor = 0x%x, n = %d!!!!\n", ef, n);
       sqlite3_bind_blob(stmt, 2, uInt8_pw, n+1, NULL);
       sqlite3_bind_int(stmt, 3, islogin);
       #else
       sqlite3_bind_text(stmt, 1, name, -1, NULL);
       sqlite3_bind_text(stmt, 2, passward, -1, NULL);
       sqlite3_bind_int(stmt, 3, islogin);
       #endif
    
       rc = sqlite3_step(stmt);  /* 执行sql 语句 */
       if(rc != SQLITE_DONE){
            printf("[QQ DB]qq_sql3_insert() ==> sqlite3_step ERROR return %d", rc);
            pthread_mutex_unlock(&s_sidb_write_lock);
             sqlite3_finalize(stmt);
            return -1;
       }
    }
    else{
       printf("[QQ DB]EitDb is NULL!!!!!!!!!\n");
       sqlite3_finalize(stmt);
       pthread_mutex_unlock(&s_sidb_write_lock);
       return -1;
    }

     sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_delete(const char* name)
{
    if(!name){
        printf("[QQ DB]qq_sql3_delete() name is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "delete from %s where name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]%s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 删除表中数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_check_by_name(
    const char* name,
    char* passward)
{
    if(!name || !passward){
        printf("[QQ DB]name or passward == NULL\n");
        return -1;
    }

    char sql[256];
    sprintf(sql, "select passward from %s where " \
                    "name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
       int len = sqlite3_column_bytes(stmt, 0);
       printf("[QQ DB]^^^^^^^^^ Get passward len = %d\n", len);
       #if NEED_ENCRYPT
       const unsigned char *srcData = NULL;
       srcData = (unsigned char*)sqlite3_column_blob(stmt, 0);
       unsigned char ef = srcData[len-1];
       printf("[QQ DB]encrypt_factor = 0x%x, len = %d!!!!\n", ef, len);
       qq_un_encrypt(passward, srcData, len - 1, ef);
       printf("[QQ DB] passward = %s\n", passward);
       #else
       const char *srcData;
       srcData = (char*)sqlite3_column_text(stmt, 0);
       memcpy(passward, srcData, len * sizeof(char));
       passward[len] = '\0';
       #endif
    }
    else{
        printf("[QQ DB]!!!!!!!!! Can't get event data by name from qq Table\n");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
}

static int qq_sql3_search_by_name(const char* name)
{
    if(!name){
        printf("[QQ DB]name == NULL\n");
        return -1;
    }

    char sql[256];
    sprintf(sql, "select * from %s where " \
                    "name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]###  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
       sqlite3_finalize(stmt);
       return 0;
    }
    else{
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

static int qq_sql3_rename(const char* oldname, const char* newname)
{
    if(!oldname || !newname){
        printf("[QQ DB]qq_sql3_rename() oldname or newname is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set name = '%s' where name == '%s';",\
              QQ_TABLE_NAME, newname, oldname);

    printf("[QQ DB]###  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 更新数据表中的数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_change_passward(const char* name, const char* new_passward)
{
    if(!name || !new_passward){
        printf("[QQ DB]qq_sql3_change_passward() name or new_passward is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set passward = '%s' where name == '%s';",\
              QQ_TABLE_NAME, new_passward, name);

    printf("[QQ DB]  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 更新数据表中的数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_set_login_flag(const char* name, const EM_LOGIN_F islogin)
{
    if(!name ){
        printf("[QQ DB]qq_sql3_set_login_flag() name is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set islogin = %d where name == '%s';",\
              QQ_TABLE_NAME, islogin, name);

    printf("[QQ DB]  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 更新数据表中的数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_get_login_flag(const char* name, EM_LOGIN_F *islogin)
{
    if(!name || !islogin){
        printf("[QQ DB]name or islogin == NULL\n");
        return -1;
    }

    char sql[256];
    sprintf(sql, "select islogin from %s where " \
                    "name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
       int len = sqlite3_column_bytes(stmt, 0);
       printf("[QQ DB]^^^^^^^^^ Get passward len = %d\n", len);
    
       *islogin = (EM_LOGIN_F)sqlite3_column_int(stmt, 0);
       printf("[QQ DB] get loginflag = %d\n", *islogin);
      
    }
    else{
        printf("[QQ DB]!!!!!!!!! Can't get event data by name from qq Table\n");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
}


/* 同时修改密码及登录状态 */
static int qq_sql3_ch_pwd_logflg(
    const char* name,
    const char* new_passward,
    const EM_LOGIN_F islogin)
{
    if(!name || !new_passward){
        printf("[QQ DB]qq_sql3_change_passward() name or new_passward is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set passward = '%s', islogin = %d where name == '%s';",\
              QQ_TABLE_NAME, new_passward, islogin, name);

    printf("[QQ DB]  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 更新数据表中的数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_get_all_users(UL* userHead)
{
    if(!userHead) return -1;
    
    char sql[256];
    sprintf(sql, "select name, islogin from %s;", QQ_TABLE_NAME);

    printf("[QQ DB]  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    while(rc == SQLITE_ROW){
       int len = sqlite3_column_bytes(stmt, 0);
       printf("[QQ DB]^^^^^^^^^ Get name len = %d\n", len);

       UIfo node;
       const unsigned char *srcName = NULL;
       srcName = (unsigned char*)sqlite3_column_text(stmt, 0);
       memcpy(node.userId, srcName, len * sizeof(char));
       node.userId[len] = '\0';
       const EM_LOGIN_F islogin = (EM_LOGIN_F)sqlite3_column_int(stmt, 1);
       node.logFlag = islogin;
       addUserToList(userHead, node);

       rc = sqlite3_step(stmt);
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
}

EM_RES myQQ_Init()
{
    if(qq_sql3_init() < 0)
        return QQ_FAIL;
    return QQ_OK;
}

void myQQ_DeInit()
{
    /* 数据库close */
    int rc = sqlite3_close(qqUserInfoDb);
    if(rc != SQLITE_OK){
        if(rc == SQLITE_BUSY){ /* 可能是调用了sqlite3_prepare 而没有finalize掉资源 */
            printf("[QQ DB]database is busy, can't close !!!!!\n");

            /*强制finalize掉资源*/
            sqlite3_stmt * stmt;
            while((stmt = sqlite3_next_stmt(qqUserInfoDb, NULL)) != NULL){
                sqlite3_finalize(stmt);
            }
            rc = sqlite3_close(qqUserInfoDb);
            if(rc != SQLITE_OK){
                printf("[QQ DB]close EitDb database fatal error!!!!!!!!!!!!!!\n");
            }
        }
        else{
            printf("[QQ DB]close database fail!!!!!\n");
        }
   }
   return;
}

EM_RES myQQ_LoginCheck(const char* name, const char* passward)
{
    EM_RES res = QQ_FAIL;
    char pSavePassWard[24];
    int rc = qq_sql3_check_by_name(name, pSavePassWard);
    if(rc < 0){
        printf("[QQ DB]User %s do not exist !!!!\n", name);
        return QQ_USER_UN_EXIST;
    }

    if(strcmp(passward, pSavePassWard) != 0){
        printf("[QQ DB]passward wrong!!!( [needcheck:%s] is not equal to [save:%s])\n", passward, pSavePassWard);
        return QQ_PASSWARD_WRONG;
    }

    rc = qq_sql3_set_login_flag(name, LOGIN_YES);
    if(rc < 0){
        printf("[QQ DB] qq_sql3_set_login_flag() fail!!!!\n");
        return QQ_FAIL;
    }
    
    return QQ_OK;
}

EM_RES myQQ_Register(
    const char* name, 
    const char* passward)
{
    int rc = qq_sql3_search_by_name(name);
    if(rc == 0){
        printf("[QQ DB]user %s is exist !!!!\n", name);
        return QQ_USER_EXIST;
    }
    
    rc = qq_sql3_insert(name, passward, LOGIN_NO);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_insert() fail!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_UnRegister(const char* name)
{
    int rc = qq_sql3_delete(name);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_delete() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_ChangeName(const char* oldName, const char* newName)
{
    int rc = qq_sql3_search_by_name(newName);
    if(rc == 0){
        printf("[QQ DB]myQQ_ChangeName() newName is exist!!!!\n");
        return QQ_OK;
    }
    
    rc = qq_sql3_rename(oldName, newName);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_rename() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_ChangePassward(const char* name, const char* newPassward)
{
    #if NEED_ENCRYPT  /* 如果密码需要加密 */
    int rc = qq_sql3_delete(name);
    if(rc < 0){
        printf("[QQ DB]myQQ_ChangePassward==>qq_sql3_delete() fail\n");
        return QQ_FAIL;
    }
    
    EM_LOGIN_F login_flag = LOGIN_NO;
    rc = qq_sql3_get_login_flag(name, &login_flag);
    if(rc < 0){
        printf("[QQ DB]myQQ_ChangePassward==>qq_sql3_get_login_flag() fail\n");
        return QQ_FAIL;
    }
    
    rc = qq_sql3_insert(name, newPassward, login_flag);
    if(rc < 0){
        printf("[QQ DB]myQQ_ChangePassward==>qq_sql3_insert() fail\n");
        return QQ_FAIL;
    }
    #else
    int rc = qq_sql3_change_passward(name, newPassward);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_change_passward() fail!!!!!\n");
        return QQ_FAIL;
    }
    #endif
    return QQ_OK;
}

EM_RES myQQ_SetUserLogin(const char* name)
{
    int rc = qq_sql3_set_login_flag(name, LOGIN_YES);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_set_login_flag() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_SetUserLogout(const char* name)
{
    int rc = qq_sql3_set_login_flag(name, LOGIN_NO);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_set_login_flag() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_UserIsLogin(const char* name)
{
    EM_LOGIN_F islogin;
    int rc = qq_sql3_get_login_flag(name, &islogin);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_get_login_flag() fail!!!!!\n");
        return QQ_FAIL;
    }

    if(islogin == LOGIN_NO){
        return QQ_USER_UNLOGIN;
    }
    
    return QQ_USER_LOGIN;
}

EM_RES myQQ_GetAllUsers(UL* userHead)
{
    if(userHead == NULL){
        printf("[QQ DB] userHead == NULL!!\n");
        return QQ_FAIL;
    }

    qq_sql3_get_all_users(userHead);
    return QQ_OK;
}
#else
static int qq_sql3_create_table()
{
    char sql[512];
    const char* tableName = QQ_TABLE_NAME;
    #if NEED_ENCRYPT
    sprintf(sql, "create table if not exists %s (" \
        "name text not null," \
        "passward blob not null);", tableName);
    #else
    sprintf(sql, "create table if not exists %s (" \
        "name text not null," \
        "passward text not null);", tableName);
    #endif

    printf("[QQ DB] %s\n", sql);

    /* 创建表 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            return -1;
        }else{
            printf("[QQ DB]Table %s created successfully\n", tableName);
        }
    }
    return 0;
}

static int qq_sql3_init()
{
    char* zErrMsg;
    int rc = 0;
    /* 数据库创建或打开 */
    rc = sqlite3_open(QQ_DB_NAME, &qqUserInfoDb);

    if( rc ){
       printf("[QQ DB]Can't open database: %s\n", sqlite3_errmsg(qqUserInfoDb));
       return -1;
    }else{
       printf("[QQ DB]Opened database successfully\n");
    }

#if SQLITE_WAL_MODE
    rc = sqlite3_exec(qqUserInfoDb, "PRAGMA journal_mode=WAL;", NULL, NULL, &zErrMsg);
    if( rc != SQLITE_OK ){
        printf("[QQ DB]SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    /* 指定提交的大小，默认为1KB */
    //rc = sqlite3_exec(db, "PRAGMA wal_autocheckpoint=100;", NULL, 0, &zErrMsg);
#endif  //#if SQLITE_WAL_MODE

    pthread_mutex_init(&s_sidb_write_lock, NULL);

    pthread_mutex_lock(&s_sidb_write_lock);
    rc  = qq_sql3_create_table();
    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_insert(const char* name, const char* passward)
{
    int rc = 0;
    if(!name || !passward){
        printf("[QQ DB]qq_sql3_insert() name or passward is NULL!!!\n");
        return -1;
    }
    
    pthread_mutex_lock(&s_sidb_write_lock);

    char sql[512];
    sprintf(sql, "insert into %s (name,passward) " \
                            "values (:name,:passward);", QQ_TABLE_NAME);

    printf("[QQ DB]%s\n", sql);
                 
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL); /* 准备语句对象 */
    if(rc != SQLITE_OK){
        if(qqUserInfoDb == NULL){
            printf("[QQ DB]qqUserInfoDb == NULL!!!!\n");
        }
        printf("[QQ DB]prepare Insert sql stmt Fail!!!!!!!!, rc = %d\n", rc);
         sqlite3_finalize(stmt);
        return -1;
    }
    
    if(qqUserInfoDb != NULL){
       sqlite3_reset(stmt);
      
       /* 将需要插入的数据 绑定到stmt对象 */
       #if  NEED_ENCRYPT
       sqlite3_bind_text(stmt, 1, name, -1, NULL);
       unsigned char ef = qq_encrypt_factor_get();
       unsigned char uInt8_pw[1024];
       int n = qq_encrypt(passward, uInt8_pw, ef);
       uInt8_pw[n] = ef;  /* 动态加密因子存储在密码的最后一个字节 */
       printf("[QQ DB]encrypt_factor = 0x%x, n = %d!!!!\n", ef, n);
       sqlite3_bind_blob(stmt, 2, uInt8_pw, n+1, NULL);
       #else
       sqlite3_bind_text(stmt, 1, name, -1, NULL);
       sqlite3_bind_text(stmt, 2, passward, -1, NULL);
       #endif
    
       rc = sqlite3_step(stmt);  /* 执行sql 语句 */
       if(rc != SQLITE_DONE){
            printf("[QQ DB]qq_sql3_insert() ==> sqlite3_step ERROR return %d", rc);
            pthread_mutex_unlock(&s_sidb_write_lock);
             sqlite3_finalize(stmt);
            return -1;
       }
    }
    else{
       printf("[QQ DB]EitDb is NULL!!!!!!!!!\n");
       sqlite3_finalize(stmt);
       pthread_mutex_unlock(&s_sidb_write_lock);
       return -1;
    }

     sqlite3_finalize(stmt);
    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_delete(const char* name)
{
    if(!name){
        printf("[QQ DB]qq_sql3_delete() name is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "delete from %s where name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]%s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 删除表中数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_check_by_name(const char* name, char* passward)
{
    if(!name || !passward){
        printf("[QQ DB]name or passward == NULL\n");
        return -1;
    }

    char sql[256];
    sprintf(sql, "select passward from %s where " \
                    "name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
       int len = sqlite3_column_bytes(stmt, 0);
       printf("[QQ DB]^^^^^^^^^ Get passward len = %d\n", len);
       #if NEED_ENCRYPT
       const unsigned char *srcData = NULL;
       srcData = (unsigned char*)sqlite3_column_blob(stmt, 0);
       unsigned char ef = srcData[len-1];
       printf("[QQ DB]encrypt_factor = 0x%x, len = %d!!!!\n", ef, len);
       qq_un_encrypt(passward, srcData, len - 1, ef);
       printf("[QQ DB] passward = %s\n", passward);
       #else
       const char *srcData;
       srcData = (char*)sqlite3_column_text(stmt, 0);
       memcpy(passward, srcData, len * sizeof(char));
       passward[len] = '\0';
       #endif
    }
    else{
        printf("[QQ DB]!!!!!!!!! Can't get event data by name from qq Table\n");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
}

static int qq_sql3_search_by_name(const char* name)
{
    if(!name){
        printf("[QQ DB]name == NULL\n");
        return -1;
    }

    char sql[256];
    sprintf(sql, "select * from %s where " \
                    "name == '%s';", QQ_TABLE_NAME, name);

    printf("[QQ DB]###  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
       sqlite3_finalize(stmt);
       return 0;
    }
    else{
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

static int qq_sql3_rename(const char* oldname, const char* newname)
{
    if(!oldname || !newname){
        printf("[QQ DB]qq_sql3_rename() oldname or newname is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set name = '%s' where name == '%s';",\
              QQ_TABLE_NAME, newname, oldname);

    printf("[QQ DB]###  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 删除表中数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_change_passward(const char* name, const char* new_passward)
{
    if(!name || !new_passward){
        printf("[QQ DB]qq_sql3_change_passward() name or new_passward is NULL\n");
        return -1;
    }
    char sql[256];
    sprintf(sql, "update %s set passward = '%s' where name == '%s';",\
              QQ_TABLE_NAME, new_passward, name);

    printf("[QQ DB]  %s\n", sql);
    pthread_mutex_lock(&s_sidb_write_lock);
    /* 删除表中数据 */
    if(qqUserInfoDb != NULL){
        int rc = 0;
        char* zErrMsg = NULL;
        rc = sqlite3_exec(qqUserInfoDb, sql, NULL, NULL, &zErrMsg);
        if( rc != SQLITE_OK ){
            printf("[QQ DB]SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            pthread_mutex_unlock(&s_sidb_write_lock);
            return -1;
        }else{
            pthread_mutex_unlock(&s_sidb_write_lock);
            return 0;
        }
    }
    else{
        pthread_mutex_unlock(&s_sidb_write_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sidb_write_lock);
    return 0;
}

static int qq_sql3_get_all_users(UL* userHead)
{
    if(!userHead) return -1;
    
    char sql[256];
    sprintf(sql, "select name from %s;", QQ_TABLE_NAME);

    printf("[QQ DB]  %s\n", sql);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(qqUserInfoDb, sql, strlen(sql), &stmt, NULL);
    int rc = 0;
    rc = sqlite3_step(stmt);
    while(rc == SQLITE_ROW){
       int len = sqlite3_column_bytes(stmt, 0);
       UIfo node;
       memset(&node, 0, sizeof(UIfo));
       const unsigned char *srcName = NULL;
       srcName = (unsigned char*)sqlite3_column_text(stmt, 0);
       memcpy(node.userId, srcName, len * sizeof(char));
       node.userId[len] = '\0';
       addUserToList(userHead, node);

       rc = sqlite3_step(stmt);
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
}

EM_RES myQQ_Init()
{
    //if(qq_sql3_init() < 0)
      //  return QQ_FAIL;

    qq_check_return(qq_sql3_init() < 0, QQ_FAIL);

    return QQ_OK;
}

void myQQ_DeInit()
{
    /* 数据库close */
    int rc = sqlite3_close(qqUserInfoDb);
    if(rc != SQLITE_OK){
        if(rc == SQLITE_BUSY){ /* 可能是调用了sqlite3_prepare 而没有finalize掉资源 */
            printf("[QQ DB]database is busy, can't close !!!!!\n");

            /*强制finalize掉资源*/
            sqlite3_stmt * stmt;
            while((stmt = sqlite3_next_stmt(qqUserInfoDb, NULL)) != NULL){
                sqlite3_finalize(stmt);
            }
            rc = sqlite3_close(qqUserInfoDb);
            if(rc != SQLITE_OK){
                printf("[QQ DB]close EitDb database fatal error!!!!!!!!!!!!!!\n");
            }
        }
        else{
            printf("[QQ DB]close database fail!!!!!\n");
        }
   }
   return;
}

EM_RES myQQ_LoginCheck(const char* name, const char* passward)
{
    EM_RES res = QQ_FAIL;
    char pSavePassWard[24];
    int rc = qq_sql3_check_by_name(name, pSavePassWard);
    if(rc < 0){
        printf("[QQ DB]User %s do not exist !!!!\n", name);
        return QQ_USER_UN_EXIST;
    }

    if(strcmp(passward, pSavePassWard) != 0){
        printf("[QQ DB]passward wrong!!!( [needcheck:%s] is not equal to [save:%s])\n", passward, pSavePassWard);
        return QQ_PASSWARD_WRONG;
    }

    return QQ_OK;
}

EM_RES myQQ_Register(const char* name, const char* passward)
{
    int rc = qq_sql3_search_by_name(name);
    if(rc == 0){
        printf("[QQ DB]user %s is exist !!!!\n", name);
        return QQ_USER_EXIST;
    }
    
    rc = qq_sql3_insert(name, passward);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_insert() fail!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_UnRegister(const char* name)
{
    int rc = qq_sql3_delete(name);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_delete() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_ChangeName(const char* oldName, const char* newName)
{
    int rc = qq_sql3_search_by_name(newName);
    if(rc == 0){
        printf("[QQ DB]myQQ_ChangeName() newName is exist!!!!\n");
        return QQ_OK;
    }
    
    rc = qq_sql3_rename(oldName, newName);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_rename() fail!!!!!\n");
        return QQ_FAIL;
    }
    return QQ_OK;
}

EM_RES myQQ_ChangePassward(const char* name, const char* newPassward)
{
    #if NEED_ENCRYPT  /* 如果密码需要加密 */
    int rc = qq_sql3_delete(name);
    if(rc < 0){
        printf("[QQ DB]myQQ_ChangePassward==>qq_sql3_delete() fail\n");
        return QQ_FAIL;
    }
    rc = qq_sql3_insert(name, newPassward);
    if(rc < 0){
        printf("[QQ DB]myQQ_ChangePassward==>qq_sql3_insert() fail\n");
        return QQ_FAIL;
    }
    #else
    int rc = qq_sql3_change_passward(name, newPassward);
    if(rc < 0){
        printf("[QQ DB]qq_sql3_change_passward() fail!!!!!\n");
        return QQ_FAIL;
    }
    #endif
    return QQ_OK;
}

EM_RES myQQ_GetAllUsers(UL* userHead)
{
    if(userHead == NULL){
        printf("[QQ DB] userHead == NULL!!\n");
        return QQ_FAIL;
    }

    qq_sql3_get_all_users(userHead);
    return QQ_OK;
}

EM_RES myQQ_IsSamePassward(const char* name, const char* passward)
{
    EM_RES res = QQ_FAIL;
    char pSavePassWard[24];
    int rc = qq_sql3_check_by_name(name, pSavePassWard);
    if(rc < 0){
        printf("[QQ DB]User %s do not exist !!!!\n", name);
        return QQ_USER_UN_EXIST;
    }

    if(strcmp(passward, pSavePassWard) == 0){
        printf("[QQ DB]passward is equal!!!!\n");
        return QQ_OK;
    }

    return res;
}


#endif

