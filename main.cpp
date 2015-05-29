#include "stdafx.h"
#include <iostream>
#include <vector>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

using namespace std;

#define HANDLE_FLAG_OVERLAPPED 1
#define HANDLE_FLAG_IGNOREEOF 2
#define HANDLE_FLAG_UNITBUFFER 4

struct handle_input {
    /*
     * Copy of the handle_generic structure.
     */
    HANDLE h;			       /* the handle itself */

    /*
     * Data set at initialisation and then read-only.
     */
    int flags;

    /*
     * Data set by the input thread before signalling ev_to_main,
     * and read by the main thread after receiving that signal.
     */
    char buffer[4096];		       /* the data read from the handle */
    DWORD len;			       /* how much data that was */
    int readerr;		       /* lets us know about read errors */
};

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

/*
 * The actual thread procedure for an input thread.
 */
static DWORD WINAPI handle_input_threadfunc(void *param)
{
    struct handle_input *ctx = (struct handle_input *) param;
    OVERLAPPED ovl, *povl;
    HANDLE oev = NULL;
    int readret, readlen;
	string data;


    if (ctx->flags & HANDLE_FLAG_OVERLAPPED) {
		povl = &ovl;
		oev = CreateEvent(NULL, TRUE, FALSE, NULL);
    } else {
		povl = NULL;
    }

    if (ctx->flags & HANDLE_FLAG_UNITBUFFER)
		readlen = 1;
    else
		readlen = sizeof(ctx->buffer);
	
    while (1) {
		if (povl) {
			memset(povl, 0, sizeof(OVERLAPPED));
			povl->hEvent = oev;
		}
		readret = ReadFile(ctx->h, ctx->buffer,readlen, &ctx->len, povl);
		if (!readret)
			ctx->readerr = GetLastError();
		else {
			data.append(1, ctx->buffer[0]);
			ctx->readerr = 0;
		}
		if (povl && !readret && ctx->readerr == ERROR_IO_PENDING) {
			if(!data.empty()) {
				int pos = data.find("NMBR");
				if (pos != string::npos) {
					string number = ltrim(data.substr(pos + 6));
					cout<< number.substr(0, number.size()-2);
				}
				data.clear();
			}
			WaitForSingleObject(povl->hEvent, INFINITE);
			readret = GetOverlappedResult(ctx->h, povl, &ctx->len, FALSE);
			if (!readret)
				ctx->readerr = GetLastError();
			else
				ctx->readerr = 0;
		}

		if (!readret) {
			/*
			 * Windows apparently sends ERROR_BROKEN_PIPE when a
			 * pipe we're reading from is closed normally from the
			 * writing end. This is ludicrous; if that situation
			 * isn't a natural EOF, _nothing_ is. So if we get that
			 * particular error, we pretend it's EOF.
			 */
			if (ctx->readerr == ERROR_BROKEN_PIPE)
				ctx->readerr = 0;
			ctx->len = 0;
		}

		if (readret && ctx->len == 0 &&
			(ctx->flags & HANDLE_FLAG_IGNOREEOF))
			continue;

	
		if (!ctx->len)
			break;
	}

		if (povl)
			CloseHandle(oev);

		return 0;
	
}

int  main() {
	
	HANDLE serport = ::CreateFile(L"COM4", GENERIC_READ | GENERIC_WRITE, 0, NULL,
			     OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (serport == INVALID_HANDLE_VALUE) {
		std::cout << "Unable to open serial port"  <<std::endl;
	}

	handle_input * i = new handle_input();
	i->flags = HANDLE_FLAG_OVERLAPPED | HANDLE_FLAG_IGNOREEOF | HANDLE_FLAG_UNITBUFFER;
	i->h = serport;

	::CreateThread(NULL,0,handle_input_threadfunc,i,0,NULL);
	while(1) {}
}