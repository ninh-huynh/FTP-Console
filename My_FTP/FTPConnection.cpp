﻿#include "stdafx.h"
#include "FTPConnection.h"

int getRelyCode(const char *relyMsg)
{
	int RelyCode;
	sscanf_s(relyMsg, "%d", &RelyCode);
	return RelyCode;
}

void splitLineToVector(const char *src, vector<CString> &des)
{
	static CString buff;
	buff += src;
	int length = buff.Find("\r\n");

	while (length != -1)
	{
		des.push_back(buff.Left(length));
		buff.Delete(0, length + 2);
		length = buff.Find("\r\n");
	}
}

void splitLineToQueue(queue<CString> &des, CString src)
{
	static CString buff;
	buff += src;
	int length = buff.Find("\r\n");

	while (length != -1)
	{
		des.push(buff.Left(length + 2));
		buff.Delete(0, length + 2);
		length = buff.Find("\r\n");
	}
}

BOOL FTPConnection::InitDataSock()
{
	char msg[MAX_MSG_BUF]{ 0 };
	int msgSz;

	if (isPassive)
	{
		sprintf_s(msg, "PASV\r\n");
		dataTrans.Create();
	}
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
	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return false;
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) == SOCKET_ERROR)
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

void FTPConnection::close_data_sock()
{
	if (!isPassive)
		dataSock.Close();
	dataTrans.Close();
}

bool FTPConnection::is_extension(const CString& s)
{
	return s.Find(_T("*.")) || s.Find(".*");
}

CString FTPConnection::get_path(const CString & fullPath)
{
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];

	errno_t err;
	err = _splitpath_s(fullPath.GetString(), drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);

	// Thiếu cả tên ổ cứng và thư mục chứa -> dùng đường dẫn hiện tại
	if (CString(drive).IsEmpty() && CString(dir).IsEmpty())
		return currentDir + "\\";

	//Chỉ thiếu tên ổ cứng -> thư mục con của đường dẫn hiện tại
	if (CString(drive).IsEmpty())
		return currentDir + "\\" + (CString)dir;

	//Đủ cả tên ổ cứng và thư mục
	return (CString)drive + (CString)dir;
}

CString FTPConnection::get_fileName_with_Ext(const CString & fullPath)
{
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	errno_t err;
	err = _splitpath_s(fullPath.GetString(), NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

	return (CString)fname + (CString)ext;
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
	SetCurrentDirectory(currentDir);
	isPassive = false;
	currentMode = Mode::ASCII;
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
	char msg[MAX_MSG_BUF];
	int msgSz;

	if (!controlSock.Create()) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
	}

	if (!controlSock.Connect(IPAddr, 21)) {
		switch (controlSock.GetLastError())
		{
		case WSAETIMEDOUT:
			strcpy_s(msg, "Connection time out.\r\n");				// Tự thoát khi quá thời gian chờ
			break;
		case WSAEINVAL:
			strcpy_s(msg, "Unknown host.\r\n");						// Tự kiểm tra địa chỉ IP hợp lệ
			break;
		}

		outputControlMsg.push(CString(msg));
		controlSock.Close();
		return FALSE;
	}

	// Thông báo đã kết nối thành công tới IPAddr
	sprintf_s(msg, "Connected to %s.\n", IPAddr);
	outputControlMsg.push(CString(msg));

	UINT clientControlPort;
	controlSock.GetSockName(clientIPAddr, clientControlPort);

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
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
	char msg[MAX_MSG_BUF];
	int msgSz;

	sprintf_s(msg, "USER %s\r\n", userName);
	msgSz = strlen(msg);
	msg[msgSz] = '\0';
	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
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
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
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
	char msg[MAX_MSG_BUF];
	int msgSz;

	sprintf_s(msg, "QUIT\r\n");
	msgSz = strlen(msg);

	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
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
BOOL FTPConnection::ListAllFile(const CString& remote_dir, const CString& local_file)
{
	char msg[MAX_MSG_BUF + 1]{ 0 }, data[MAX_TRANSFER + 1]{ 0 };
	int msgSz, dataSz = 0;

	sprintf_s(msg, "NLST %s\r\n", remote_dir.GetString());

	// thiết lập kết nối nếu ở chế độ active (mặc định)
	if (!InitDataSock())
		return FALSE;

	// Gửi lệnh NLST cho server
	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	// thiết lập kết nối cho data socket
	if (isPassive)
	{
		CString server_ip;
		UINT dummy;
		controlSock.GetPeerName(server_ip, dummy);
		dataTrans.Connect(server_ip, server_data_port);
	}
	else
	{
		if (!dataSock.Listen(1))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(msg);
			dataSock.Close();
			return FALSE;
		}

		if (!dataSock.Accept(dataTrans))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(msg);
			dataSock.Close();
			return FALSE;
		}
	}


	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	// Trường hợp mà nhận đc cả hai thông điệp 150 ... \r\n và 126... \r\n
	// ->Tách ra rồi đưa vào hàng đợi
	splitLineToQueue(outputControlMsg, msg);
	// kiểm tra code phản hồi, "150 Opening ASCII mode data connection for NLIST ..."
	if (getRelyCode(msg) != 150)
	{
		close_data_sock();
		return FALSE;
	}

	// đã nhận được phản hồi đúng từ server
	int blockSz = 0;
	chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
	
	// bắt đầu nhận phản hồi từ data port (thông tin danh sách file)
	while ((blockSz = dataTrans.Receive(data, MAX_TRANSFER, 0)) > 0)
	{
		data[blockSz] = '\0';
		splitLineToVector(data, outputMsg);
		dataSz += blockSz;
	}
	
	chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
	
	close_data_sock();
	// Lấy thử thông điệp cuối cùng -> Đã nhận "226 ...\r\n" thì thoát
	if (CString(msg).Find("226") != -1)
		goto printTransferSpeed;

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
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

	printTransferSpeed:
	sprintf_s(msg, "My ftp: %d bytes received in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count());
	outputControlMsg.push(CString(msg));
	return TRUE;
}

BOOL FTPConnection::ListAllDirectory(const char * remote_dir, const char * local_file)
{
	unique_lock<mutex> guard(m, defer_lock);
	char msg[MAX_MSG_BUF];
	int msgSz;

	//============ Không cho phép truy cập outputControlMsg từ bên ngoài
	guard.lock();
	if (InitDataSock() == false)
		return false;

	CString serverIP;
	UINT serverPort;
	if (isPassive)
	{
		int A, B, C, D, a, b;
		sscanf_s(outputControlMsg.front().GetString(), "227 %*s %*s %*s (%d,%d,%d,%d,%d,%d)", &A, &B, &C, &D, &a, &b);
		serverIP.Format("%d.%d.%d.%d", A, B, C, D);
		serverPort = 256 * a + b;
	}
	guard.unlock();
	//============= Mở truy cập outputControlMsg

	sprintf_s(msg, "LIST %s\r\n", remote_dir);
	msgSz = strlen(msg);
	if (controlSock.Send(&msg, msgSz) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if (!isPassive)
	{
		if (!dataSock.Listen())
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			return FALSE;
		}
		else
		{
			if (!dataSock.Accept(dataTrans))
			{
				sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
				outputControlMsg.push(CString(msg));
				return FALSE;
			}
		}
	}
	else
	{
		if (!dataTrans.Connect(serverIP.GetString(), serverPort))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			return false;
		}
	}


	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return false;
	}

	msg[msgSz] = '\0';
	// Trường hợp mà nhận đc cả hai thông điệp 150 ... \r\n và 126... \r\n
	// ->Tách ra rồi đưa vào hàng đợi
	splitLineToQueue(outputControlMsg, msg);

	int relyCode = getRelyCode(msg);
	if (relyCode != 150 && relyCode != 125)
		return false;

	// Nhận data ở cả active và passive
	char data[MAX_TRANSFER] = { 0 };
	int dataSz = 0, blockSz;
	
	chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
	
	//Gửi lần lượt từng block data lên server
	while ((blockSz = dataTrans.Receive(data, MAX_TRANSFER - 1)) > 0)
	{
		data[blockSz] = '\0';
		splitLineToVector(data, outputMsg);
		dataSz += blockSz;
	}
	chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
	close_data_sock();
	
	// Lấy thử thông điệp cuối cùng -> Đã nhận "226 ...\r\n" thì thoát
	if (CString(msg).Find("226") != -1)
	{
		sprintf_s(msg, "My ftp: %d bytes received in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count());
		outputControlMsg.push(CString(msg));
		return true;
	}
	// Nếu vẫn chưa nhận thông điệp "226 ...\r\n"
	// -> Nhận cho đủ
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return false;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(msg);
	
	sprintf_s(msg, "My ftp: %d bytes received in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count());
	outputControlMsg.push(CString(msg));
	return true;
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
		outputControlMsg.push(msg);
		return TRUE;
	}

	msg.Format("%s: File not found\n", directory);
	outputControlMsg.push(msg);
	return FALSE;
}

BOOL FTPConnection::CreateDir(const char * directory)
{
	char msg[MAX_MSG_BUF];
	int msgSz;

	sprintf_s(msg, "MKD %s\r\n", directory);
	msgSz = strlen(msg);

	if (controlSock.Send(&msg, msgSz) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	msg[msgSz] = '\0';							//Thêm kí tự NULL vào cuối thông điệp nhận được
	outputControlMsg.push(CString(msg));

	if (getRelyCode(msg) != 257)				// "257 ... directory created." 
		return FALSE;
	return TRUE;
}

BOOL FTPConnection::PutFile(const CString& localFile, const CString& remoteFile)
{
	unique_lock<mutex> guard(m, defer_lock);
	// the mutex is not locked yet!
	char msg[MAX_MSG_BUF];
	int msgSz;

	ifstream file;
	file.open(localFile.GetString(), ios::binary);
	if (!file.is_open())										// Nếu không mở được file -> tb lỗi, thoát ngay
	{
		sprintf_s(msg, "%s: File not found.\r\n", localFile);
		outputControlMsg.push(msg);
		return FALSE;
	}

	//============ Không cho phép truy cập outputControlMsg từ bên ngoài
	guard.lock();
	// Mở được file
	if (InitDataSock() == false)
		return false;

	CString serverIP;
	UINT serverPort;
	if (isPassive)
	{
		int A, B, C, D, a, b;
		sscanf_s(outputControlMsg.front().GetString(), "227 %*s %*s %*s (%d,%d,%d,%d,%d,%d)", &A, &B, &C, &D, &a, &b);
		serverIP.Format("%d.%d.%d.%d", A, B, C, D);
		serverPort = 256 * a + b;
	}
	guard.unlock();
	//============= Mở truy cập outputControlMsg
	if (!remoteFile.IsEmpty())
		sprintf_s(msg, "STOR %s\r\n", remoteFile.GetString());
	else
		sprintf_s(msg, "STOR %s\r\n", get_fileName_with_Ext(localFile).GetString());

	msgSz = strlen(msg);
	if (controlSock.Send(&msg, msgSz) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		file.close();
		return FALSE;
	}

	if (!isPassive)
	{
		if (!dataSock.Listen())
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			return FALSE;
		}
		
		if (!dataSock.Accept(dataTrans))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			return FALSE;
		}
	}
	else
	{
		if (!dataTrans.Connect(serverIP.GetString(), serverPort))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			close_data_sock();
			return FALSE;
		}
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	splitLineToQueue(outputControlMsg, msg);

	int relyCode = getRelyCode(msg);
	if (relyCode != 150 && relyCode != 125)
	{
		file.close();
		close_data_sock();
		return FALSE;
	}
	// Gửi data ở cả active và passive
	char data[MAX_TRANSFER - 1] = { 0 };
	int blockSz, dataSz = 0;

	chrono::steady_clock::time_point t1 = chrono::steady_clock::now();

	while (!file.eof())
	{
		file.read(data, MAX_TRANSFER - 1);
		blockSz = file.gcount();
		dataTrans.Send(&data, blockSz);
		dataSz += blockSz;
	}

	chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	file.close();
	close_data_sock();
	
	// Lấy thử thông điệp cuối cùng -> Đã nhận "226 ...\r\n" thì thoát
	if (CString(msg).Find("226") != -1)
	{
		sprintf_s(msg, "My ftp: %d bytes sent in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count());
		outputControlMsg.push(CString(msg));
		return TRUE;
	}

	// Nếu vẫn chưa nhận thông điệp "226 ...\r\n"
	// -> Nhận cho đủ
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(msg);
	sprintf_s(msg, "My ftp: %d bytes sent in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count());
	outputControlMsg.push(CString(msg));
	return TRUE;
}

//void FTPConnection::PrintControlMsg()
//{
//	while (!this->outputControlMsg.empty())
//	{
//		cout << this->outputControlMsg.front();
//		this->outputControlMsg.pop();
//	}
//
//	while (!this->outputMsg.empty())
//	{
//		cout << this->outputMsg.front();
//		this->outputMsg.pop();
//	}
//}

void FTPConnection::SetPassiveMode()
{
	isPassive = !isPassive;
	if (isPassive)
		outputControlMsg.push("Passive mode on.\n");
	else
		outputControlMsg.push("Passive mode off.\n");
}

BOOL FTPConnection::GetFile(const CString& remote_file_name, const CString& local_file_name)
{
	CFile local_file;
	char msg[MAX_MSG_BUF + 1]{ 0 }, data[MAX_TRANSFER]{ 0 };
	int msgSz, dataSz = 0;

	// khởi tạo kết nối data
	if (!InitDataSock())
		return FALSE;

	// Gửi lệnh lấy 1 file (RETR remote-file)
	sprintf_s(msg, "RETR %s\r\n", remote_file_name.GetString());
	if (controlSock.Send(msg, strlen(msg), 0) <= 0)
	{
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	// thiết lập kết nối cho data socket
	if (isPassive)
	{
		CString server_ip;
		UINT dummy;
		controlSock.GetPeerName(server_ip, dummy);
		dataTrans.Connect(server_ip, server_data_port);
	}
	else
	{
		if (!dataSock.Listen(1))
		{
			sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
			outputControlMsg.push(msg);
			dataSock.Close();
			return FALSE;
		}

		dataSock.Accept(dataTrans);
	}

	// nhận phản hồi từ control port
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	splitLineToQueue(outputControlMsg, msg);

	if (getRelyCode(msg) != 150)		//"150 Opening BINARY mode data connection for ...."
	{
		close_data_sock();
		return FALSE;
	}

	// đã nhận được phản hồi đúng từ server
	// tạo file rỗng tại client
	if (!local_file.Open(local_file_name, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
	{
		sprintf_s(msg, "Cannot create local file: %s\n", local_file_name);
		outputControlMsg.push(msg);
		return FALSE;
	}

	chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
	int blockSz;
	// bắt đầu nhận phản hồi từ data port (dữ liệu của file)
	while ((blockSz = dataTrans.Receive(data, MAX_TRANSFER, 0)) > 0)
	{
		local_file.Write(data, blockSz);
		dataSz += blockSz;
	}

	chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	local_file.Close();
	close_data_sock();

	// Lấy thử thông điệp cuối cùng -> Đã nhận "226 ...\r\n" thì thoát
	if (CString(msg).Find("226") != -1)
		goto printTransferSpeed;

	// Nếu vẫn chưa nhận thông điệp "226 ...\r\n"
	// -> Nhận cho đủ
	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF)) <= 0) {
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}
	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 226)
		return FALSE;

	printTransferSpeed:
	sprintf_s(msg, "My ftp: %d bytes received in %.2f (s) %.2f (KB/s).\r\n", dataSz, time_span.count(), ((float)dataSz / 1024) / time_span.count()); 
	outputControlMsg.push(CString(msg));
	return TRUE;
}

BOOL FTPConnection::GetMultipleFiles(const vector<CString>& remote_file_names)
{
	vector<CString> remote_file_paths;
	vector<CString> file_names;
	char dir[256], buf[256];

	// lần lượt tìm các file trong danh sách, nếu có 1 file nào đó không tồn tại => return
	for (const auto& elm : remote_file_names)
	{
		if (!ListAllFile(elm, ""))
		{
			while (outputControlMsg.size() > 1)		// vứt hết tất cả ra ngoại trừ thông báo sau cùng
				outputControlMsg.pop();
			sprintf_s(buf, "Cannot find \"%s\".\n", elm);
			outputControlMsg.push(buf);

			return FALSE;
		}
		else
		{
			_splitpath_s(elm, NULL, 0, dir, 255, NULL, 0, NULL, 0);
			file_names.insert(end(file_names), begin(outputMsg), end(outputMsg));
			for (const auto& file_name : outputMsg)
			{
				remote_file_paths.push_back(dir + file_name);
			}
		}

		// vứt hết thông tin trong danh sách file ra
		outputMsg.clear();
	}

	// clear outputControlMsg
	//decltype(outputControlMsg) dummy;
	//swap(outputControlMsg, dummy);

	// set chế độ truyền
	SetMode(currentMode);

	// bắt đầu tải từng file trên server
	for (int i = 0; i < remote_file_paths.size(); ++i)
	{
		if (!GetFile(remote_file_paths[i], file_names[i]))
			return FALSE;
	}

	return TRUE;
}

BOOL FTPConnection::SetMode(FTPConnection::Mode mode)
{
	char msg[MAX_MSG_BUF]{ 0 };
	int msgSz;

	sprintf_s(msg, "TYPE %c\r\n", mode == ASCII ? 'A' : 'I');
	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 200)	// "200 Type set to ..."
		return FALSE;

	currentMode = mode;
	return TRUE;
}

BOOL FTPConnection::ChangeRemoteWorkingDir(const CString& dir)
{
	char msg[MAX_MSG_BUF + 1]{ 0 };
	int msgSz;

	sprintf_s(msg, "CWD %s\r\n", dir.GetString());

	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 250)	//"250 Directory successfully changed"
		return FALSE;

	return true;
}

BOOL FTPConnection::DeleteRemoteFile(const CString& remote_file_name)
{
	char msg[MAX_MSG_BUF + 1]{ 0 };
	int msgSz;

	sprintf_s(msg, "DELE %s\r\n", remote_file_name.GetString());

	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 250)	//"250 Deleted file ..."
		return FALSE;

	return TRUE;
}

BOOL FTPConnection::DeleteRemoteMultipleFiles(const vector<CString>& remote_file_names)
{
	vector<CString> remote_file_paths;
	char dir[256], buf[256];

	// lần lượt tìm các file trong danh sách, nếu có 1 file nào đó không tồn tại => return
	for (const auto& elm : remote_file_names)
	{
		if (!ListAllFile(elm, ""))
		{
			while (outputControlMsg.size() > 1)		// vứt hết tất cả ra ngoại trừ thông báo sau cùng
				outputControlMsg.pop();
			sprintf_s(buf, "Cannot find \"%s\".\n", elm);
			outputControlMsg.push(buf);

			return FALSE;
		}
		else
		{
			_splitpath_s(elm, NULL, 0, dir, 255, NULL, 0, NULL, 0);
			for (const auto& file_name : outputMsg)
			{
				remote_file_paths.push_back(dir + file_name);
			}
		}

		// vứt hết thông tin trong danh sách file ra
		outputMsg.clear();
	}

	// clear outputControlMsg
	decltype(outputControlMsg) dummy;
	swap(outputControlMsg, dummy);

	// set chế độ truyền
	SetMode(currentMode);

	// bắt đầu xóa từng file trên server
	for (int i = 0; i < remote_file_paths.size(); ++i)
	{
		if (!DeleteRemoteFile(remote_file_paths[i]))
			return FALSE;
	}

	return TRUE;
}

BOOL FTPConnection::RemoveRemoteDir(const CString& remote_dir)
{
	char msg[MAX_MSG_BUF + 1]{ 0 };
	int msgSz;

	sprintf_s(msg, "RMD %s\r\n", remote_dir.GetString());

	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 250)	//"250 Directory removed"
		return FALSE;

	return TRUE;
}

/**
 * Hàm cài đặt lệnh PWD
 * 
 * @return thành công/thất bại
 */
BOOL FTPConnection::PrintRemoteWorkingDir()
{
	char msg[MAX_MSG_BUF + 1]{ 0 };
	int msgSz;

	strcpy_s(msg, "PWD\r\n");

	if (controlSock.Send(msg, strlen(msg), 0) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	if ((msgSz = controlSock.Receive(msg, MAX_MSG_BUF, 0)) == SOCKET_ERROR)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 257)	//"257 /..."
		return FALSE;

	return TRUE;
}

BOOL FTPConnection::PutMultipleFiles(const vector<CString>& localFile)
{
	// Tham số tổng quát
	// *.ext	/path/*.ext		/path/file_name.ext		file_name.ext
	BOOL bRet = TRUE;

	char sPath[256];
	char msg[MAX_MSG_BUF];
	for (auto it : localFile)
	{

		sprintf_s(sPath, "%s%s", get_path(it).GetString(), get_fileName_with_Ext(it).GetString());

		WIN32_FIND_DATA fdFile;
		HANDLE hFind = nullptr;

		//Tìm thấy tên file thỏa trong thư mục
		if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
		{
			sprintf_s(msg, "%s : File not found\r\n", it.GetString());
			outputControlMsg.push(CString(msg));
			bRet = FALSE;
			continue;
		}

		do
		{
			// Nếu tồn tại 1 file ko upload được -> ghi nhận vào biến bRet, tiếp tục thử các file tiếp theo
			bRet = PutFile(get_path(it) + fdFile.cFileName, "") && bRet;
		} while (FindNextFile(hFind, &fdFile));
		FindClose(hFind);
	}

	return bRet;
}

void FTPConnection::Help()
{

	outputMsg = 
	{ "?", "open", "user", "ls", "dir", "quit", 
		"get", "mget", "put", "lcd", "delete", 
		"mdelete", "passive", "close","mkdir", 
		"mput", "ascii", "binary", "cd", "rmdir", "pwd", "!", "help"};

	sort(begin(outputMsg), end(outputMsg), [](const CString& a, const CString& b) {return a < b; });

}
