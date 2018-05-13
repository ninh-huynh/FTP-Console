#include "stdafx.h"
#include "FTPConnection.h"

int getRelyCode(const char *relyMsg)
{
	int RelyCode;
	sscanf_s(relyMsg, "%d", &RelyCode);
	return RelyCode;
}

void FTPConnection::InitDataSock(bool isPass, CSocket &)
{
}

FTPConnection::FTPConnection()
{
	if (!AfxSocketInit()) {
		outputControlMsg.push(CString("Failed to Initialize Sockets"));
	}
}

FTPConnection::~FTPConnection()
{
}

BOOL FTPConnection::OpenConnection(char * IPAddr)
{
	return 0;
}

BOOL FTPConnection::Close()
{
	return 0;
}

BOOL FTPConnection::ListAllFile(char * fileExt)
{
	return 0;
}

void FTPConnection::GetOutputControlMsg(queue<CString>& des)
{
	des = outputControlMsg;
}
