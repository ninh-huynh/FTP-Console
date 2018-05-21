#include "stdafx.h"
#include "FTPConnection.h"

int getRelyCode(const char *relyMsg)
{
	int RelyCode;
	sscanf_s(relyMsg, "%d", &RelyCode);
	return RelyCode;
}

BOOL FTPConnection::InitDataSock(bool isPass)
{
	char msg[MAX_BUFFER]{0};
	int msgSz;

	if (isPass)
	{
		sprintf_s(msg, "PASV\r\n");
	}
	else
	{
		dataSock.Create();
		CString ip;
		UINT port;
		controlSock.GetSockName(ip, port);
		ip.Replace(_T('.'), _T(','));
		sprintf_s(msg, "PORT %s,%d,%d\r\n", ip.GetString(), port / 256, port % 256);

	}

	// gửi lệnh PASV/PORT
	if (controlSock.Send(msg, strlen(msg), 0) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return false;
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return false;
	}

	// kiểm tra code từ phản hồi của server
	int code = getRelyCode(msg);
	if (code != (isPass ? 227 : 200))		// "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)", "200 PORT command successful"
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return false;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	return true;
}

FTPConnection::FTPConnection()
{
	if (!AfxSocketInit()) {
		outputControlMsg.push(CString("Failed to Initialize Sockets"));
	}

	// Lấy đường dẫn home dir trên windows C:/User/[Username]
	SHGetSpecialFolderPath(NULL, currentDir.GetBuffer(MAX_PATH), CSIDL_PROFILE, FALSE);
	currentDir.ReleaseBuffer();
}

FTPConnection::~FTPConnection()
{
	Close();
}


/*
* Thiết lập kết nối đến FTP server port 21
*
* @IPAddr là địa chỉ của FTP server
*
* @Các thông báo lỗi, thông điệp nhận được sẽ được
* cất toàn bộ vào hàng đợi outputControlMsg
*/
BOOL FTPConnection::OpenConnection(const char * IPAddr)
{
	char msg[MAX_BUFFER];
	int msgSz;

	if (!controlSock.Create()) {
		sprintf_s(msg, "%s %d", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
	}

	auto isValidIPAddr = [](const char *IPAddr) {
		UINT a, b, c, d;
		return sscanf_s(IPAddr, "%d.%d.%d.%d", &a, &b, &c, &d) == 4;
	};

	if (!isValidIPAddr(IPAddr)) {
		outputControlMsg.push(CString("Unknow host\n"));
		return FALSE;
	}

	if (!controlSock.Connect(IPAddr, 21)) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	sprintf_s(msg, "Connected to %s.\n", IPAddr);
	outputControlMsg.push(CString(msg));

	UINT clientControlPort;
	controlSock.GetSockName(clientIPAddr, clientControlPort);

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0){
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 220)
		return FALSE;
	return TRUE;
}


/*
* Đăng nhập vào FTP server (chỉ khi đã thiết lập kết nối thành công)
* 
* @userName : tên người dùng
* @userPass : mật khẩu
* 
* @Khi được gọi thì chỉ truyền 1 trong 2 tham số trên, ko truyền
* thời do có tương tác sau lần nhập tên
*
* @Các thông báo lỗi, thông điệp nhận được sẽ được
* cất toàn bộ vào hàng đợi outputControlMsg
*/

BOOL FTPConnection::LogIn(const char * userName, const char * userPass)
{
	char msg[MAX_BUFFER];
	int msgSz;

	sprintf_s(msg, "USER %s\r\n", userName);
	msgSz = strlen(msg);
	msg[msgSz] = '\0';
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';						//Thêm kí tự NULL vào cuối thông điệp nhận được
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 331)
		return FALSE;
	
	sprintf_s(msg, "PASS %s\r\n", userPass);
	msgSz = strlen(msg);
	msg[msgSz] = '\0';
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';							//Thêm kí tự NULL vào cuối thông điệp nhận được
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 230)
		return FALSE;
	return TRUE;
}

/*
* Kết thúc 1 phiên làm việc với FTP Server
*
* @Các thông báo lỗi, thông điệp nhận được sẽ được
* cất toàn bộ vào hàng đợi outputControlMsg
*/
BOOL FTPConnection::Close()
{
	char msg[MAX_BUFFER];
	int msgSz;

	sprintf_s(msg, "QUIT\r\n");
	msgSz = strlen(msg);
	
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	msg[msgSz] = '\0';							//Thêm kí tự NULL vào cuối thông điệp nhận được
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

BOOL FTPConnection::LocalChangeDir(char * directory)
{
	CString msg;
	BOOL bRet = TRUE;

	if (*directory != '\0')
		bRet = SetCurrentDirectory(directory);
	
	if (bRet == TRUE)
	{
		if (*directory != '\0')
			currentDir = directory;
		msg.Format("Local directory now %s", currentDir);
		outputMsg.push(msg);
		return TRUE;
	}

	msg.Format("%s: File not found", directory);
	outputMsg.push(msg);
	return FALSE;
}
