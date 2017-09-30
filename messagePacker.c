#include "messagePacker.h"
#include <string.h>
#include <stdarg.h>
/**
* @brief function: ��Ϣ�����
* @param[in] msgData: �����洢�������Ϣ
* @return void.
*/
void apl_msgPacker(char* msgData, ...)
{
	if(msgData == NULL)
		return;
	va_list vaList;
	void* para;
	char* funParam = msgData;
	va_start(vaList, msgData);
	int len = va_arg(vaList, int);
	while((len != 0) && (len != -1)){
		para = va_arg(vaList, void*);
		memcpy(funParam, para, len);
		funParam += len;
		len = va_arg(vaList, int);
	}
	va_end(vaList);
	return;
}

/**
* @brief function: ��Ϣ�����
* @param[in] msgData: �����洢�������Ϣ
* @return void.
*/
void apl_msgUnPacker(char* msgData, ...)
{
	if(msgData == NULL)
		return;
	va_list vaList;
	void* para;
	char* funParam = msgData;
	va_start(vaList, msgData);
	int len = va_arg(vaList, int);
	while((len != 0) && (len != -1)){
		para = va_arg(vaList, void*);
		memcpy(para, funParam, len);
		funParam += len;
		len = va_arg(vaList, int);
	}
	va_end(vaList);
	return;
}


