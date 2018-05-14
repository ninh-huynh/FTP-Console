// My_FTP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "My_FTP.h"
#include "FTPConnection.h"

#ifdef _DEBUG
#define _new DEBUG_NEW
#endif

#define MAX_LINE_BUF 1024

// The one and only application object

CWinApp theApp;

using namespace std;

bool isEnoughArg(int nArg, const char *cmd){
	if (!strcmp(cmd, "open") 
		|| !strcmp(cmd,"user")
		|| !strcmp(cmd,"cd")
		|| !strcmp(cmd,"get")
		|| !strcmp(cmd,"put")
		|| !strcmp(cmd,"mkdir")
		|| !strcmp(cmd,"rmdir")
		|| !strcmp(cmd,"delete")
		) {
		if (nArg == 1)
			return false;
		if (nArg == 2)
			return true;
	}
	return false;
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
			char line[MAX_LINE_BUF];
			char cmd[MAX_LINE_BUF];
			char arg[MAX_LINE_BUF];
			int nArg;
			BOOL isConnected, isLogin;

			if (argc == 2) {
				strcpy_s(arg, MAX_LINE_BUF, argv[1]);
				goto connectToServer;
			}

			while (TRUE){
				cout << "My ftp> ";
				cin.getline(line, MAX_LINE_BUF);

				// Thử lấy 2 chuỗi con từ từ chuỗi nhập ban đầu
				// Số chuỗi con lấy được lưu trong nArg
				if (nArg = sscanf_s(line,"%s %s",cmd, (unsigned)_countof(cmd),arg, (unsigned)_countof(arg))){

					if (!strcmp(cmd, "open")) {
						if (!isEnoughArg(nArg,cmd)){
							cout << "To: ";
							cin.getline(arg, MAX_LINE_BUF);
						}

						connectToServer:
						isConnected = myFTP.OpenConnection(arg);

						while (!myFTP.outputControlMsg.empty())
						{
							cout << myFTP.outputControlMsg.front();
							myFTP.outputControlMsg.pop();
						}

						if (!isConnected)
							continue;
						
						goto login;
						//Login
					}
					else if (!strcmp(cmd, "user")) {
						if (!isEnoughArg(nArg, cmd)) {
							login:
							cout << "Username: ";
							cin.getline(arg, MAX_LINE_BUF);
						}
						isLogin = myFTP.LogIn(arg, nullptr);
						
						while (!myFTP.outputControlMsg.empty())
						{
							cout << myFTP.outputControlMsg.front();
							myFTP.outputControlMsg.pop();
						}

						if (!isLogin)
							continue;
						//Nếu như đã xác nhận userName okie
						//Tiếp tục nhập pass

						// Giấu kí tự mật khẩu nhập 
						HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
						DWORD mode = 0;
						GetConsoleMode(hstdin, &mode);
						SetConsoleMode(hstdin, mode & (~ENABLE_ECHO_INPUT));

						cout << "Password: ";
						cin.getline(arg, MAX_LINE_BUF);
						cout << '\n';

						// Hiện lại nhập bình thường
						SetConsoleMode(hstdin, mode | ENABLE_ECHO_INPUT);

						isLogin = myFTP.LogIn(nullptr, arg);
						
						while (!myFTP.outputControlMsg.empty())
						{
							cout << myFTP.outputControlMsg.front();
							myFTP.outputControlMsg.pop();
						}
					}
					else if (!strcmp(cmd, "quit")) {
						myFTP.Close();
						
						while (!myFTP.outputControlMsg.empty())
						{
							cout << myFTP.outputControlMsg.front();
							myFTP.outputControlMsg.pop();
						}
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
