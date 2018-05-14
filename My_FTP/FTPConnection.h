#pragma once
#include <afxsock.h>
#include <queue>
#include <cstdio>

#define MAX_BUFFER 1461

using namespace std;

class FTPConnection
{
private:
	CSocket controlSock;
	CString clientIPAddr;
	//char recvMsg[MAX_BUFFER];
	bool isPassive;
	queue<CString> outputMsg;
	queue<CString> outputControlMsg;
	void InitDataSock(bool isPass, CSocket &);
public:
	FTPConnection();
	~FTPConnection();
	BOOL OpenConnection(const char* IPAddr);
	BOOL LogIn(const char *userName, const char *userPass);
	BOOL Close();
	BOOL ListAllFile(char* fileExt);
	void GetOutputControlMsg(queue<CString> &des);
};

