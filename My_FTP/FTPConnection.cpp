﻿#include "stdafx.h"
#include "FTPConnection.h"

int getRelyCode(const char *relyMsg)
{
	int RelyCode;
	sscanf_s(relyMsg, "%d", &RelyCode);
	return RelyCode;
}

BOOL FTPConnection::InitDataSock()
{
	char msg[MAX_BUFFER]{0};
	int msgSz;

	if (isPassive)
		sprintf_s(msg, "PASV\r\n");
	else
	{
		dataSock.Create();
		CString ip;
		UINT port;
		dataSock.GetSockName(ip, port);
		ip = clientIPAddr;
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

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);
	// kiểm tra code từ phản hồi của server
	if (getRelyCode(msg) != (isPassive ? 227 : 200))		// "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)", "200 PORT command successful"
		return false;

	// lưu data port của server ở chế độ PASSIVE
	if (isPassive)
	{
		cmatch match_results;
		regex expr("(\\d{1,3})\\,(\\d{1,3})\\)");

		regex_search(msg, match_results, expr);
		server_data_port = stoi(match_results[1]) * 256 + stoi(match_results[2]);
	}

	return true;
}

bool FTPConnection::isPath(const CString& s)
{
	return (s.Find(_T('/')) != -1) || (s.Find('\\') != -1);
}

FTPConnection::FTPConnection()
{
	if (!AfxSocketInit()) {
		outputControlMsg.push(CString("Failed to Initialize Sockets"));
	}

	// Lấy đường dẫn home dir trên windows C:/User/[Username]
	// CSIDL_PROFILE chính là hằng số đại diện đường dẫn trên
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

/**
 * - Hàm lấy thông tin danh sách file từ remote-directory (lệnh "LS")
 * - Cú pháp sử dụng (ví dụ):
 *   + ls abc.*
 *   + ls *.abc
 *   + ls /a/b/c abc.txt	(lưu vào currentDir trên client)
 *   + ls /a/b/c path		(lưu vào path)
 * 
 * 
 * @fileExt điều kiện lọc các file
 * @remote_dir thư mục muốn xem trên server
 * @local_file file/đường dẫn lưu thông tin ở client
 * 
 * @return TRUE/FALSE
 */
BOOL FTPConnection::ListAllFile(const CString& fileExt, const CString& remote_dir, const CString& local_file)
{
	// mở file xuất (nếu có)
	ofstream file;
	ostream *os;
	if (!local_file.IsEmpty())
	{
		// kiểm tra local_file là đường dẫn hay tên file
		CString full_path = isPath(local_file) ? local_file : (currentDir + '\\' + local_file);
		file.open(full_path.GetString());
		os = &file;
	}
	else
	{
		os = &cout;
	}

	char msg[MAX_BUFFER]{ 0 };
	int msgSz;

	sprintf_s(msg, "NLST %s %s\r\n", fileExt.GetString(), remote_dir.GetString());

	// thiết lập kết nối nếu ở chế độ active (mặc định)
	InitDataSock();
	this->PrintControlMsg();
	

	// Gửi lệnh NLST cho server
	if (controlSock.Send(msg, strlen(msg), 0) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	// kiểm tra code phản hồi, "150 Opening ASCII mode data connection for NLIST (773 bytes)"
	if (getRelyCode(msg) != 150)
	{
		outputControlMsg.push(msg);
		return FALSE;
	}

	// đã nhận được phản hồi đúng từ server
	msg[msgSz] = '\0';
	std::cout << msg;

	// thiết lập kết nối cho data socket
	if (isPassive)
	{
		CString server_ip;
		UINT dummy;
		controlSock.GetPeerName(server_ip, dummy);
		dataTrans.Create();
		dataTrans.Connect(server_ip, server_data_port);
	}
	else
	{
		if (!dataSock.Listen(1))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(msg);
			return FALSE;
		}

		dataSock.Accept(dataTrans);
	}

	// bắt đầu nhận phản hồi từ data port (thông tin danh sách file)
	while ((msgSz = dataTrans.Receive(msg, MAX_BUFFER, 0)) > 0)
	{
		msg[msgSz] = '\0';
		*os << msg;
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	// kiểm tra code của phàn hồi, 
	if (getRelyCode(msg) != 226)	// "226 Transfer complete ...."
		return FALSE;


	if (!isPassive)
		dataSock.Close();
	dataTrans.Close();

	isPassive = FALSE;		// trở lại chế độ active (mặc định)

	return TRUE;
}

BOOL FTPConnection::LocalChangeDir(const char * directory)
{
	CString msg;
	BOOL bRet = FALSE;

	// Nếu truyền chuỗi rỗng thì lấy thư mục hiện hành
	// Mặc định là C:\Users\[Username]
	// currentDir được lấy ở class constructor

	if (*directory != '\0')
		bRet = SetCurrentDirectory(currentDir + "\\" + directory) || SetCurrentDirectory(directory);
	else
		bRet = SetCurrentDirectory(currentDir);
	
	if (bRet == TRUE)
	{
		GetCurrentDirectory(MAX_PATH, currentDir.GetBuffer(MAX_PATH));
		currentDir.ReleaseBuffer();
		msg.Format("Local directory now %s\n", currentDir);
		outputMsg.push(msg);
		return TRUE;
	}

	msg.Format("%s: File not found\n", directory);
	outputMsg.push(msg);
	return FALSE;
}

void FTPConnection::PrintControlMsg()
{
	while (!this->outputControlMsg.empty())
	{
		cout << this->outputControlMsg.front();
		this->outputControlMsg.pop();
	}

	while (!this->outputMsg.empty())
	{
		cout << this->outputMsg.front();
		this->outputMsg.pop();
	}
}

void FTPConnection::SetPassiveMode()
{
	isPassive = true;
	outputControlMsg.push("Passive mode on.\n");
}
