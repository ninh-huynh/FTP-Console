#pragma once
#include <afxsock.h>
#include <queue>
#include <cstdio>
#include <regex>

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

	bool isPassive;											//Đang ở mode Active hay Passive
public:
	FTPConnection();
	~FTPConnection();
	BOOL OpenConnection(const char* IPAddr);				//Thiết lập kết nối tới Server
	BOOL LogIn(const char *userName, const char *userPass);	//Truyền cả 2 tham số, đã sửa lại!
	BOOL InitDataSock(bool isPass);							//Thiết lập kết nối TCP
	BOOL Close();
	BOOL ListAllFile(char* fileExt);
	BOOL LocalChangeDir(const char* directory);
	queue<CString> outputMsg;								
	queue<CString> outputControlMsg;						//Hàng đợi chứa thông điệp nhận được, lỗi,...
};															//trong quá trình gọi hàm


