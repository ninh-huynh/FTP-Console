// My_FTP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "My_FTP.h"
#include "FTPConnection.h"
#include <map>
#include <regex>

#ifdef _DEBUG
#define _new DEBUG_NEW
#endif

#define MAX_LINE_BUF 1024

// The one and only application object

CWinApp theApp;

using namespace std;

queue<CString> argQueue;

enum CMD {
	m_open,
	m_user,
	m_ls,
	m_dir,
	m_get,
	m_put,
	m_delete,
	m_mdelete,
	m_mkdir,
	m_rmdir,
	m_mget,
	m_mput,
	m_quit,
	m_close,
	m_cd,
	m_pwd,
	m_passive,
	m_lcd,
};

class key_cmp
{
private:
	static CString tolower(CString s)
	{
		return s.MakeLower();
	}
public:
	bool operator ()(const CString& a, const CString& b) const
	{
		return tolower(a) < tolower(b);
	}
};

map<CString, CMD, key_cmp> mCMD = {
	{"open",CMD::m_open},
	{"user",CMD::m_user},
	{"ls",CMD::m_ls},
	{"dir",CMD::m_dir},
	{"quit",CMD::m_quit},
	{"get",CMD::m_get},
	{"put",CMD::m_put},
	{"delete", CMD::m_delete},
	{"mdelete", CMD::m_mdelete},
	{"passive", CMD::m_passive}

};

bool isEnoughArg(int nArg, const char *cmd) 
{
	switch (mCMD[cmd])
	{
	case m_pwd:
	case m_passive:
	case m_quit:
		return nArg == 0;
	case m_open:
	case m_cd:
	case m_mkdir:
	case m_rmdir:
	case m_delete: 
	case m_lcd:
		return nArg == 1;
	case m_user:
	case m_get:
	case m_put:
	case m_ls:
	case m_dir:
		return nArg == 2;
	default:
		return true;
	}
}

/*
* Tách từng chuỗi con có trong line
* Dấu phân cách giữa các chuỗi con là khoảng trắng
* Nếu chuỗi con có dấu nháy kép bao bọc "Chuoi con"
* -> Tách theo dấu nháy kép
* Đưa vào hàng đợi argQueue
*/
void LoadArgIntoQueue(char *line)
{
	if (*line == '\0')
		return;

	char *p;
	char delimeter;
	size_t len = strlen(line);
	p = line + 1;
	*p == '\"' ? delimeter = '\"' : delimeter = ' ';
	if (*p == '\"')
		p++;
	while (*p != '\0')
	{
		char *q = p;
		while (*q != delimeter && *q != '\0')
			q++;
		*q = '\0';
		argQueue.push(p);
		if (q >= line + len || q + 1 >= line + len || q + 2 >= line + len)
			break;
		delimeter == '\"' ? p = q + 2 : p = q + 1;
		*p == '\"' ? delimeter = '\"' : delimeter = ' ';
		if (*p == '\"')
			p++;
	}
}

/*
* In các thông báo từ hàng đợi outputControlMsg
*/
void printOutput(FTPConnection &ftp)
{
	while (!ftp.outputControlMsg.empty()) 
	{
		cout << ftp.outputControlMsg.front();
		ftp.outputControlMsg.pop();
	}

	while (!ftp.outputMsg.empty())
	{
		cout << ftp.outputMsg.front();
		ftp.outputMsg.pop();
	}
}

/*
* KT có phải là lệnh hợp lệ hay không?
*/
bool isValidCommand(CString cmd)
{
	return mCMD.find(cmd) != mCMD.end();
}

bool isRequireConnected(CString cmd)
{
	switch (mCMD[cmd])
	{
	case m_passive: case m_lcd: case m_open:
		return false;
	default:
		return true;
	}
}

/*
* @ KT có đủ 'tham số' cho 'lệnh' hay chưa
* @ Nếu chưa đủ thì tùy vào 'lệnh' gì mà bổ sung cho đủ
*   Hàng đợi argQueue sẽ chứa các tham số
*/
void GetEnoughArg(CString cmd)
{
	char line[MAX_LINE_BUF];

	if (isEnoughArg(argQueue.size(), cmd))
		return;

	switch (mCMD[cmd])
	{
	case m_open:
		cout << "To: ";
		cin.getline(line, MAX_LINE_BUF);
		argQueue.push(line);
		break;

	case m_user:
		switch (argQueue.size())
		{
		case 0:
			cout << "User: ";
			cin.getline(line, MAX_LINE_BUF);
			argQueue.push(line);
		case 1:
			HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
			DWORD mode = 0;
			GetConsoleMode(hstdin, &mode);
			SetConsoleMode(hstdin, mode & (~ENABLE_ECHO_INPUT));

			cout << "Password: ";
			cin.getline(line, MAX_LINE_BUF); cout << '\n';
			SetConsoleMode(hstdin, mode | ENABLE_ECHO_INPUT);
			argQueue.push(line);
		}
		break;

	case m_lcd:
		argQueue.push("");
	}
}

int main(int argc, char* argv[])
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // initialize MFC and print and error on failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: change error code to suit your needs
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
		else
		{
			// TODO: code your application's behavior here.
			FTPConnection myFTP;
			char line[MAX_LINE_BUF]{};
			char cmd[MAX_LINE_BUF];
			char *arg;
			
			BOOL isConnected, isLogin, keepLoop;
			CString serverAddr, userName, password;
			isConnected = FALSE;
			isLogin = FALSE;
			keepLoop = TRUE;

			if (argc == 2) {
				argQueue.push(argv[1]);
				goto connectToServer;
			}

			while (keepLoop){
				cout << "My ftp> ";											// Nhập 1 'dòng'
				cin.getline(line, MAX_LINE_BUF);							// dòng = 'lệnh' + 'các tham số'
				nextCommand:
				if (sscanf_s(line,"%s",cmd, (unsigned)_countof(cmd)))		// Thử lấy 'lệnh' từ 1 dòng
				{						
					if (!isValidCommand(cmd))								// Nếu 'lệnh' không hợp lệ -> Xuất tb, bỏ qua 1 vòng lặp
					{
						cout << "Invalid command!\n";
						continue;
					}
					if (isRequireConnected(cmd) && !isConnected)
					{
						cout << "Not connected.\n";
						continue;
					}
					arg = line + strlen(cmd);
					LoadArgIntoQueue(arg);									// Đưa 'các tham số' -> hàng đợi argQueue

#ifdef _DEBUG
					{
						int i = 1;
						auto q = argQueue;
						while (!q.empty())
						{
							cout << "arg " << i++ << ": " << q.front() << '\n';
							q.pop();
						}
					}
#endif
																			
					GetEnoughArg(cmd);										// Bổ sung cho đủ 'các tham số' phục vụ cho 'lệnh'
				
					switch (mCMD[cmd]) 
					{
					case m_open:
					connectToServer:
						serverAddr = argQueue.front(); argQueue.pop();
						isConnected = myFTP.OpenConnection(serverAddr);
						printOutput(myFTP);
						if (!isConnected)
							continue;
						strcpy_s(line, "user");
						goto nextCommand;

					case m_user:
						userName = argQueue.front(); argQueue.pop();
						password = argQueue.front(); argQueue.pop();
						isLogin = myFTP.LogIn(userName, password);
						printOutput(myFTP);
						break;

					case m_passive:
						myFTP.InitDataSock(false);
						printOutput(myFTP);
						break;

					case m_quit:
						isConnected =  !myFTP.Close();
						printOutput(myFTP);
						keepLoop = FALSE;
						break;

					}
					
					
				}
			}
        }
    }
    else
    {
        // TODO: change error code to suit your needs
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}
