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
	char msg[MAX_BUFFER];
	if (!AfxSocketInit()) {
		outputControlMsg.push(CString("Failed to Initialize Sockets"));
	}

	if(!controlSock.Create()){
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
	}
}

FTPConnection::~FTPConnection()
{
	Close();
}

BOOL FTPConnection::OpenConnection(const char * IPAddr)
{
	auto isValidIPAddr = [](const char *IPAddr) {
		UINT a, b, c, d;
		return sscanf_s(IPAddr, "%d.%d.%d.%d", &a, &b, &c, &d) == 4;
	};

	if (!isValidIPAddr(IPAddr)) {
		outputControlMsg.push(CString("Unknow host"));
		return FALSE;
	}

	char msg[MAX_BUFFER];
	int msgSz;

	if (!controlSock.Connect(IPAddr, 21)) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	UINT clientControlPort;
	controlSock.GetSockName(clientIPAddr, clientControlPort);

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0){
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 220)
		return FALSE;
	return TRUE;
}

BOOL FTPConnection::LogIn(const char * userName, const char * userPass)
{
	
	char msg[MAX_BUFFER];
	int msgSz;
	
	sprintf_s(msg, "USER %s\r\n", userName);
	msgSz = strlen(msg);  
	msg[msgSz] = '\0';
	if (controlSock.Send(&msg, msgSz) <= 0){
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0){
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 331)
		return FALSE;

	sprintf_s(msg, "PASS %s\r\n", userPass);
	msgSz = strlen(msg);
	msg[msgSz] = '\0';
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 230)
		return FALSE;
	return TRUE;
}

BOOL FTPConnection::Close()
{

	char msg[MAX_BUFFER];
	int msgSz;
	sprintf_s(msg, "QUIT\r\n");
	msgSz = strlen(msg);
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(CString(msg));
	if (getRelyCode(msg) != 221)
		return FALSE;
	controlSock.Close();
	return TRUE;
}

BOOL FTPConnection::ListAllFile(char * fileExt)
{
	return 0;
}

void FTPConnection::GetOutputControlMsg(queue<CString>& des)
{
	des = outputControlMsg;
}
