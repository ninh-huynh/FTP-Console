#pragma once
#include <afxsock.h>
#include <queue>
#include <cstdio>
#include <regex>
#include <memory>
#include <fstream>
#include <vector>

#define MAX_MSG_BUF 255			//1461
#define MAX_TRANSFER 4096

using namespace std;

class FTPConnection
{
public:

	enum Mode
	{
		ASCII,
		BINARY
	};

private:
	CSocket controlSock;									//Socket chính, kết nối tới port 21 trên Server
	CSocket dataSock;
	CSocket dataTrans;
	CString clientIPAddr;
	CString currentDir;
	UINT server_data_port;

	bool isPassive;											//Đang ở mode Active hay Passive
	Mode currentMode;
	void close_data_sock();
	bool is_extension(const CString& s);
	CString get_file_name(const CString& s);


public:
	FTPConnection();
	~FTPConnection();
	BOOL OpenConnection(const char* IPAddr);				//Thiết lập kết nối tới Server
	BOOL LogIn(const char *userName, const char *userPass);	//Truyền cả 2 tham số, đã sửa lại!
	BOOL InitDataSock();							//Thiết lập kết nối TCP
	BOOL Close();
	BOOL ListAllFile(const CString& remote_dir, const CString& local_file);
	BOOL ListAllDirectory(const char *remote_dir, const char *local_file);
	BOOL LocalChangeDir(const char* directory);
	BOOL CreateDir(const char* directory);
	BOOL PutFile(const char *localFile, const char *remoteFile);
	vector<CString> outputMsg;
	queue<CString> outputControlMsg;						//Hàng đợi chứa thông điệp nhận được, lỗi,...
															//trong quá trình gọi hàm
															//void PrintControlMsg();
	void SetPassiveMode();
	BOOL GetFile(const CString& remote_file_name, const CString& local_file_name);
	BOOL GetMultipleFiles(const vector<CString>& remote_file_names);
	BOOL SetMode(FTPConnection::Mode mode);
	BOOL ChangeRemoteDir(const CString& dir);
	BOOL RemoveFile(const CString& remote_file_name);
	BOOL RemoveMultipleFiles(const vector<CString>& remote_file_names);
	BOOL RemoveDir(const CString& remote_dir);
};


