﻿#include "stdafx.h"
#include "FTPConnection.h"

int getRelyCode(const char *relyMsg)
{
	int RelyCode;
	sscanf_s(relyMsg, "%d", &RelyCode);
	return RelyCode;
}

void splitLineToVector(const char *src,vector<CString> &des)
{
	static CString buff;
	buff += src;
	int length = buff.Find("\r\n");
	
	while (length != -1)
	{
		des.push_back(buff.Left(length + 2));
		buff.Delete(0, length + 2);
		length = buff.Find("\r\n");
	}
}



BOOL FTPConnection::InitDataSock()
{
	char msg[MAX_BUFFER]{0};
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

void FTPConnection::close_data_sock()
{
	if (!isPassive)
		dataSock.Close();
	dataTrans.Close();
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

	auto isValidIPAddr = [](const char *IPAddr) -> bool
	{
		UINT a, b, c, d;
		if (sscanf_s(IPAddr, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
			return false;
		return a < 256 && b < 256 && c < 256 && d < 256;
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
BOOL FTPConnection::ListAllFile(const CString& remote_dir, const CString& local_file)
{
	char msg[MAX_BUFFER]{ 0 };
	int msgSz;

	sprintf_s(msg, "NLST %s\r\n", remote_dir.GetString());

	// thiết lập kết nối nếu ở chế độ active (mặc định)
	InitDataSock();

	// Gửi lệnh NLST cho server
	if (controlSock.Send(msg, strlen(msg), 0) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);
	// kiểm tra code phản hồi, "150 Opening ASCII mode data connection for NLIST ..."
	if (getRelyCode(msg) != 150)
	{
		close_data_sock();
		return FALSE;
	}

	// đã nhận được phản hồi đúng từ server

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
			dataSock.Close();
			return FALSE;
		}

		dataSock.Accept(dataTrans);
	}

	// bắt đầu nhận phản hồi từ data port (thông tin danh sách file)
	while ((msgSz = dataTrans.Receive(msg, MAX_TRANSFER - 1, 0)) > 0)
	{
		msg[msgSz] = '\0';
		splitLineToVector(msg, outputMsg);
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	// kiểm tra code của phàn hồi, 
	if (getRelyCode(msg) != 226)	// "226 Transfer complete ...."
	{
		close_data_sock();
		return FALSE;
	}

	close_data_sock();

	return TRUE;
}

BOOL FTPConnection::ListAllDirectory(const char * remote_dir, const char * local_file)
{
	char msg[MAX_BUFFER];
	int msgSz;

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

	sprintf_s(msg, "LIST %s\r\n", remote_dir);
	msgSz = strlen(msg);
	if (controlSock.Send(&msg, msgSz) <= 0)
	{
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return FALSE;
	}

	if (!isPassive)
	{
		if (!dataSock.Listen())
		{
			sprintf_s(msg, "%s %d\n", "Error code: ", dataSock.GetLastError());
			outputControlMsg.push(CString(msg));
			return FALSE;
		}
		else
		{
			if (!dataSock.Accept(dataTrans))
			{
				sprintf_s(msg, "%s %d\n", "Error code: ", dataSock.GetLastError());
				outputControlMsg.push(CString(msg));
				return FALSE;
			}
		}
	}


	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return false;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	bool isReceivedEnough = false;
	if ((CString(msg)).Find("\r\n") != msgSz - 2)
		isReceivedEnough = true;

	if (isPassive)
	{
		if (!dataTrans.Connect(serverIP.GetString(), serverPort))
		{
			sprintf_s(msg, "%s %d\n", "Error code: ", dataTrans.GetLastError());
			outputControlMsg.push(CString(msg));
			return false;
		}
	}

	int relyCode = getRelyCode(msg);
	if (relyCode != 150 && relyCode != 125)
		return false;

	// Nhận data ở cả active và passive
	char data[MAX_TRANSFER] = { 0 };
	int dataSz;
	CString buff;

	while ((dataSz = dataTrans.Receive(data, MAX_TRANSFER - 1)) > 0)
	{
		data[dataSz] = '\0';
		splitLineToVector(data, outputMsg);
	}

	dataTrans.Close();
	if (!isPassive)
		dataSock.Close();

	if (!isReceivedEnough)
	{
		if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
			sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			return false;
		}
		msg[msgSz] = '\0';
		outputControlMsg.push(msg);
	}

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
	char msg[MAX_BUFFER];
	int msgSz;

	sprintf_s(msg, "MKD %s\r\n", directory);
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

	if (getRelyCode(msg) != 257)				// "257 ... directory created." 
		return FALSE;
	return TRUE;
}

BOOL FTPConnection::PutFile(const char * localFile, const char * remoteFile)
{
	char msg[MAX_BUFFER];
	int msgSz;

	ifstream file;
	file.open(localFile, ios::binary);
	if (!file.is_open())												// Nếu không mở được file -> tb lỗi, thoát ngay
	{
		sprintf_s(msg, "%s: File not found.\r\n", localFile);
		outputControlMsg.push(msg);
		return FALSE;
	}

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

	sprintf_s(msg, "STOR %s\r\n", remoteFile);
	msgSz = strlen(msg);
	if (controlSock.Send(&msg, msgSz) <= 0)
	{
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		file.close();
		return FALSE;
	}

	if (!isPassive)
	{
		if (!dataSock.Listen())
		{
			sprintf_s(msg, "%s %d\n", "Error code: ", dataSock.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			return FALSE;
		}
		else
		{
			if (!dataSock.Accept(dataTrans))
			{
				sprintf_s(msg, "%s %d\n", "Error code: ", dataSock.GetLastError());
				outputControlMsg.push(CString(msg));
				file.close();
				return FALSE;
			}
		}
	}


	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
		sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
		outputControlMsg.push(CString(msg));
		return false;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	bool isReceivedEnough = false;
	if ((CString(msg)).Find("\r\n") != msgSz - 2)
		isReceivedEnough = true;

	if (isPassive)
	{
		if (!dataTrans.Connect(serverIP.GetString(), serverPort))
		{
			sprintf_s(msg, "%s %d\n", "Error code: ", dataTrans.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			return false;
		}
	}

	int relyCode = getRelyCode(msg);
	if (relyCode != 150 && relyCode != 125)
	{
		file.close();
		return false;
	}
	// Gửi data ở cả active và passive
	char data[MAX_TRANSFER - 1] = { 0 };
	int dataSz;
	int fileSz;
	int nLoop;

	file.seekg(0, file.end);
	fileSz = file.tellg();
	file.seekg(0, file.beg);

	nLoop = fileSz / (MAX_TRANSFER - 1);
	if (fileSz % (MAX_TRANSFER - 1) != 0)
		nLoop += 1;

	for (int i = 0; i < nLoop; i++)
	{
		if (i == nLoop - 1 && fileSz % (MAX_TRANSFER - 1) != 0)
			dataSz = fileSz % (MAX_TRANSFER - 1);
		else
			dataSz = MAX_TRANSFER - 1;
		file.read(data, MAX_TRANSFER - 1);
		dataTrans.Send(&data, dataSz);
	}
	file.close();

	dataTrans.Close();
	if (!isPassive)
		dataSock.Close();

	if (!isReceivedEnough)
	{
		if ((msgSz = controlSock.Receive(msg, MAX_BUFFER)) <= 0) {
			sprintf_s(msg, "%s %d\n", "Error code: ", controlSock.GetLastError());
			outputControlMsg.push(CString(msg));
			file.close();
			return false;
		}
		msg[msgSz] = '\0';
		outputControlMsg.push(msg);
	}

	return true;
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
	char msg[MAX_BUFFER + 1]{ 0 };
	int msgSz;

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

	// nhận phản hồi từ control port
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	if (getRelyCode(msg) != 150)		//"150 Opening BINARY mode data connection for ...."
	{
		close_data_sock();
		return FALSE;
	}

	// đã nhận được phản hồi đúng từ server
	// tạo file rỗng tại client
	if (!local_file.Open(local_file_name, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
	{
		outputControlMsg.push("Cannot create local file");
		return FALSE;
	}

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
			dataSock.Close();
			return FALSE;
		}

		dataSock.Accept(dataTrans);
	}

	// bắt đầu nhận phản hồi từ data port (dữ liệu của file)
	while ((msgSz = dataTrans.Receive(msg, MAX_TRANSFER, 0)) > 0)
	{
		local_file.Write(msg, msgSz);
	}

	// nhận phản hồi từ server
	if ((msgSz = controlSock.Receive(msg, MAX_BUFFER, 0)) <= 0)
	{
		sprintf_s(msg, "Error code: %d\n", controlSock.GetLastError());
		outputControlMsg.push(msg);
		close_data_sock();
		return FALSE;
	}

	msg[msgSz] = '\0';
	outputControlMsg.push(msg);

	// kiểm tra code của phản hồi, 
	if (getRelyCode(msg) != 226)	// "226 Transfer complete ...."
	{
		close_data_sock();
		return FALSE;
	}

	close_data_sock();

	return TRUE;
}
