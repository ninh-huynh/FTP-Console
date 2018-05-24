#pragma once
#include <afxsock.h>
#include <queue>
#include <cstdio>
#include <regex>
#include <memory>
#include <fstream>
#include <vector>

#define MAX_BUFFER 1461

using namespace std;

class FTPConnection
{
private:
	CSocket controlSock;									//Socket chính, kết nối tới port 21 trên Server
	CSocket dataSock;										
	CSocket dataTrans;
	CString clientIPAddr;		
	CString currentDir;
	UINT server_data_port;

	bool isPassive;											//Đang ở mode Active hay Passive

	bool isPath(const CString& s);
public:
	FTPConnection();
	~FTPConnection();
	BOOL OpenConnection(const char* IPAddr);				//Thiết lập kết nối tới Server
	BOOL LogIn(const char *userName, const char *userPass);	//Truyền cả 2 tham số, đã sửa lại!
	BOOL InitDataSock();							//Thiết lập kết nối TCP
	BOOL Close();
	BOOL ListAllFile(const CString& remote_dir, const CString& local_file);
	BOOL LocalChangeDir(const char* directory);
	vector<CString> outputMsg;								
	queue<CString> outputControlMsg;						//Hàng đợi chứa thông điệp nhận được, lỗi,...
															//trong quá trình gọi hàm
	//void PrintControlMsg();
	void SetPassiveMode();
};															


