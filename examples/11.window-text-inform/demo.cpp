// compile command: cl /EHsc demo.cpp /std:c++20
#define UNICODE 1
#define _UNICODE 1
#include "../../Window.hpp"
#include <format>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(linker, "/entry:mainCRTStartup /subsystem:windows")

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

BOOL EnableAllPrivileges() {
	HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) return FALSE;
	typedef NTSTATUS(WINAPI* RtlAdjustPrivilege_t)
		(ULONG Privilege, BOOL Enable, BOOL CurrentThread, PBOOL Enabled);
	RtlAdjustPrivilege_t RtlAdjustPrivilege = (RtlAdjustPrivilege_t)
		GetProcAddress(ntdll, "RtlAdjustPrivilege");
	if (!RtlAdjustPrivilege) return FALSE;
	BOOL bPrev = FALSE, bOk = TRUE;
	for (ULONG i = 1; i < 37; ++i)
		bOk &= NT_SUCCESS(RtlAdjustPrivilege(i, TRUE, FALSE, &bPrev));
	return bOk;
}

using namespace w32oop;
using namespace w32oop::foundation;
class WindowTextInformerWindow : public w32oop::Window {
public:
	WindowTextInformerWindow() : Window(L"Window Text Informer", 420, 300, 0, 0, WS_OVERLAPPEDWINDOW) {}
private:
	Static text;
	Edit inputHwnd;
	Static hexordec;
	Button btn;
	Edit resultBox;

	void onCreated() override {
		text.set_parent(this);
		text.create(L"HWND (hex):", 90, 20, 10, 12);

		inputHwnd.set_parent(this);
		inputHwnd.create(L"0x", 250, 25, 110, 10);
		inputHwnd.onChange([this](EventData &event) {
			wstring hwndStr = inputHwnd.text();
			if (hwndStr.rfind(L"0x", 0) == 0 || hwndStr.rfind(L"0X", 0) == 0) {
				hexordec.text(L"HEX");
				text.text(L"HWND (hex):");
			} else if (std::ranges::all_of(hwndStr, [](wchar_t c) { return iswdigit(c); })) {
				hexordec.text(L"DEC");
				text.text(L"HWND (dec):");
			} else {
				hexordec.text(L"INV");
				text.text(L"Invalid Input");
			}
		});

		hexordec.set_parent(this);
		hexordec.create(L"HEX", 30, 20, 370, 12, Static::STYLE | SS_CENTER);

		btn.set_parent(this);
		btn.create(L"Get Text", 380, 30, 10, 40, Button::STYLE | BS_DEFPUSHBUTTON);
		btn.onClick([this] (EventData& event) {
			doRead();
		});

		resultBox.set_parent(this);
		resultBox.create(L"[[Result will show here]]", 380, 30, 10, 80);
		resultBox.readonly(true);

		register_hot_key(true, false, false, 'W', [this](HotKeyProcData &event) {
			event.preventDefault();
			close();
		});
	}
protected:
    virtual void setup_event_handlers() override {
        
    }
private:
	void doRead() {
		wstring hwndStr = inputHwnd.text();
		// 去除"0x"或"0X"前缀后再转换十六进制字符串为数值
		int radix = 10;
		if (hwndStr.rfind(L"0x", 0) == 0 || hwndStr.rfind(L"0X", 0) == 0) {
			hwndStr = hwndStr.substr(2);
			radix = 16;
		}
		HWND hwnd = (HWND)wcstoull(hwndStr.c_str(), nullptr, radix);
		if (!IsWindow(hwnd)) {
			resultBox.text(L"Invalid Window");
			return;
		}
		LRESULT length = SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
		if (length == 0 || length < 0) {
			resultBox.text(L"");
			return;
		}
		length += 1;
		
		// 获取目标进程
		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid == 0) {
			resultBox.text(format(L"Failed - {}", GetLastError()));
			return;	
		}
		HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
		if (!hProcess) {
			resultBox.text(format(L"Failed (OpenProcess) - {}", GetLastError()));
			return;	
		}
		util::WindowRAIIHelper _([hProcess] { CloseHandle(hProcess); });
		// 分配内存
		PWSTR buffer = (LPWSTR)VirtualAllocEx(hProcess, nullptr, length * sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
		if (!buffer) {
			resultBox.text(format(L"Failed (VirtualAllocEx) - {}", GetLastError()));
			return;
		}
		util::WindowRAIIHelper _2([hProcess, buffer] { VirtualFreeEx(hProcess, buffer, 0, MEM_RELEASE); });
		// 读取窗口文本
		if (!SendMessageW(hwnd, WM_GETTEXT, length, (LPARAM)buffer)) {
			resultBox.text(format(L"Failed (SendMessageW) - {}", GetLastError()));
			return;
		}
		// 读取内存
		PWSTR str = new WCHAR[length];
		if (!ReadProcessMemory(hProcess, buffer, str, length * sizeof(WCHAR), nullptr)) {
			resultBox.text(format(L"Failed (ReadProcessMemory) - {}", GetLastError()));
			return;	
		}
		util::WindowRAIIHelper _3([str] { delete[] str; });
		resultBox.text(str);	
	}
};

int main() { 
	EnableAllPrivileges();
	WindowTextInformerWindow window;
    window.create();
    window.set_main_window();
    window.center();
    window.show();
    return window.run();
}