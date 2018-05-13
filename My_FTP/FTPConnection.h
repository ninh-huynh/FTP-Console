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
	bool isPassive;
	queue<CString> outputMsg;
	queue<CString> outputControlMsg;
	void InitDataSock(bool isPass, CSocket &);
public:
	FTPConnection();
	~FTPConnection();
	BOOL OpenConnection(char* IPAddr);
	BOOL Close();
	BOOL ListAllFile(char* fileExt);
	void GetOutputControlMsg(queue<CString> &des);
};

