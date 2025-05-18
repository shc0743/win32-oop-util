#pragma once
/*
MIT License, Copyright (c) 2025 @chcs1013
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma region internal macros
#include <string>
#include <stdexcept>
#include <utility>
#include <map>
#include <functional>
#include <mutex>
#include <windows.h>
#include <windowsx.h>
#define package namespace
#define declare {
#define endpackage }
#define declare_exception(name) class name##_exception : public runtime_error { public: name##_exception(string d) : runtime_error(d) {} name##_exception() : runtime_error(( "Exception: " # name )) {} }
#pragma endregion

package w32oop declare;

using package std;

constexpr long version = 50602000; // 5.6.2.0

declare_exception(window_not_initialized);
declare_exception(window_already_initialized);
declare_exception(window_class_registration_failure);
declare_exception(window_creation_failure);

package util declare;
wstring s2ws(const string str);
endpackage;

class Window {
private:
	static unordered_map<HWND, Window*> managed;
	static std::mutex default_font_mutex;
	static HFONT default_font;
protected:
	HWND hwnd = nullptr; // 窗口句柄

public:
#if defined(WINDOW_USE_GENERATED_CLASS) && WINDOW_USE_GENERATED_CLASS
	virtual const wstring get_class_name() const {
		auto& type = typeid(*this);
		return util::s2ws(type.name());
	}
#else
	virtual const wstring get_class_name() const = 0;
#endif

protected:
	virtual const HICON get_window_icon() const {
		return NULL;
	}
	virtual const COLORREF get_window_background_color() const {
		return RGB(255, 255, 255);
	}
	// 虚函数：判断窗口类是否已注册（控件类可覆盖此方法）
	virtual bool class_registered() const {
		WNDCLASSEXW wc{};
		return (bool)GetClassInfoExW(GetModuleHandleW(NULL), class_name.c_str(), &wc);
	}
	virtual HFONT get_font() {
		if (default_font) return default_font;
		set_default_font();
		return default_font;
	}
	static void set_default_font(HFONT font) {
		default_font_mutex.lock();
		if (default_font) DeleteObject(default_font);
		default_font = font;
		default_font_mutex.unlock();
	}
	static void set_default_font(wstring font_name = L"Consolas") {
		default_font_mutex.lock();
		if (default_font) DeleteObject(default_font);
		default_font = CreateFontW(-14, -7, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
			OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, FF_DONTCARE,
			font_name.c_str());
		default_font_mutex.unlock();
	}

private:
	wstring class_name;
	void register_class_if_needed() {
		if (!class_registered()) {
			WNDCLASSEXW wcex{};
			wcex.cbSize = sizeof(WNDCLASSEXW);

			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = StaticWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = GetModuleHandleW(NULL);
			wcex.hIcon = wcex.hIconSm = get_window_icon();
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
			wcex.hbrBackground = CreateSolidBrush(get_window_background_color());
			wcex.lpszMenuName = NULL;
			wcex.lpszClassName = class_name.c_str();

			if (!RegisterClassExW(&wcex)) {
				throw window_class_registration_failure_exception();
			}
		}
	}

protected:
	// 检查窗口句柄有效性
	void validate_hwnd() const {
		if (!hwnd) throw window_not_initialized_exception();
	}

	virtual HWND new_window() {
		return CreateWindowExW(
			setup_info->styleEx,
			class_name.c_str(),
			setup_info->title.c_str(),
			setup_info->style,
			setup_info->x, setup_info->y,
			setup_info->width, setup_info->height,
			nullptr, nullptr, GetModuleHandleW(NULL), this
		);
	}

	class setup_info_class {
	public:
		setup_info_class() : title(L""), width(0), height(0), x(0), y(0), style(WS_OVERLAPPED), styleEx(0) {}
		wstring title;
		int width, height, x, y;
		LONG style, styleEx;
	} *setup_info;

private:
	bool _created = false;

	bool is_main_window = false;

public:
	// 构造函数直接创建窗口（不自动显示）
	Window(
		const std::wstring& title,
		int width,
		int height,
		int x = 0,
		int y = 0,
		LONG style = WS_OVERLAPPED,
		LONG styleEx = WS_EX_CONTROLPARENT
	) {
		setup_info = new setup_info_class();
		setup_info->title = title;
		setup_info->width = width;
		setup_info->height = height;
		setup_info->x = x; setup_info->y = y;
		setup_info->style = style;
		setup_info->styleEx = styleEx;

		notification_router = new unordered_map<UINT, std::function<LRESULT(WPARAM, LPARAM)>>();
	}

	virtual void create() {
		if (_created) throw window_already_initialized_exception();
		class_name = get_class_name();
		register_class_if_needed();
		hwnd = new_window();
		if (!hwnd) {
			throw window_creation_failure_exception();
		}
		try {
			setup_event_handlers();
			_created = true;
			delete setup_info;
			setup_info = nullptr;
			managed[hwnd] = this;
			SendMessageW(hwnd, WM_SETFONT, (WPARAM)get_font(), 0);
			onCreated();
		}
		catch (std::exception& exc) {
			DestroyWindow(hwnd);
			hwnd = nullptr;
			throw exc;
		}
	}

	// 不允许拷贝
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	// 允许移动
	Window(Window&& other) noexcept : hwnd(other.hwnd), setup_info(other.setup_info), notification_router(other.notification_router) {
		other.hwnd = nullptr;
		other.notification_router = nullptr;
		if (hwnd) {
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			managed[hwnd] = this;
		}
	}

	Window& operator=(Window&& other) noexcept {
		// 检查自赋值
		if (this != &other) {
			// 转移所有权
			hwnd = other.hwnd;
			_created = other._created;
			setup_info = other.setup_info;
			notification_router = other.notification_router;

			// 重置源对象
			other.hwnd = nullptr;
			other._created = false;
			other.setup_info = nullptr;
			other.notification_router = nullptr;

			// 更新窗口的用户数据指针
			if (hwnd) {
				SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
				managed[hwnd] = this;
			}
		}
		return *this;
	}

	virtual ~Window() {
		if (setup_info) {
			delete setup_info;
			setup_info = nullptr;
		}
		if (notification_router) {
			delete notification_router;
			notification_router = nullptr;
		}
		if (!hwnd) return;
		if (managed.contains(hwnd)) {
			managed.erase(hwnd);
		}
		destroy();
	}

	// 操作符重载，允许自动转换为HANDLE
	operator HWND() const {
		return hwnd;
	}

	// 添加子窗口（类似appendChild）
	void append(const Window& child) {
		validate_hwnd();
		SetParent(child.hwnd, hwnd);
	}

	// 窗口操作方法
	void move(int x, int y) {
		validate_hwnd();
		SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void resize(int w, int h) {
		validate_hwnd();
		SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void center() {
		HWND parent = GetParent(hwnd);
		RECT rcParent{};
		if (parent) GetWindowRect(parent, &rcParent);

		// 取得窗口尺寸
		RECT rect;
		GetWindowRect(hwnd, &rect);
		// 获得窗口大小
		auto w = rect.right - rect.left, h = rect.bottom - rect.top;
		// 重新设置rect里的值
		if (parent) {
			auto w2 = rcParent.right - rcParent.left, h2 = rcParent.bottom - rcParent.top;
			rect.left = rcParent.left + w2 / 2 - w / 2;
			rect.top = rcParent.top + h2 / 2 - h / 2;
		} else {
			rect.left = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
			rect.top = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
		}
		// 移动窗口到指定的位置
		SetWindowPos(hwnd, HWND_TOP, rect.left, rect.top, 1, 1, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
	}

	void set_topmost(bool isTopmost) {
		validate_hwnd();
		SetWindowPos(hwnd, isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	void enable(bool enabled = true) {
		validate_hwnd();
		EnableWindow(hwnd, enabled);
	}
	void disable() {
		enable(false);
	}

	void close() {
		validate_hwnd();
		send(WM_CLOSE);
	}

	void show(int nCmdShow = SW_SHOW) {
		validate_hwnd();
		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);
	}

	void hide() {
		validate_hwnd();
		ShowWindow(hwnd, SW_HIDE);
	}

	void destroy() {
		validate_hwnd();
		DestroyWindow(hwnd);
		hwnd = nullptr;
	}

	void set_main_window(bool isMainWindow = true) {
		validate_hwnd();
		is_main_window = isMainWindow;
	}

	// 获取系统菜单
	HMENU sysmenu() const {
		validate_hwnd();
		return GetSystemMenu(hwnd, FALSE);
	}

	wstring text() const {
		validate_hwnd();
		LRESULT len = SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
		wchar_t* buffer = new wchar_t[len + 1];
		SendMessageW(hwnd, WM_GETTEXT, len + 1, (LPARAM)buffer);
		wstring text(buffer);
		delete[] buffer;
		return text;
	}

	void text(const std::wstring& text) {
		validate_hwnd();
		SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)text.c_str());
	}

	HFONT font() const {
		return reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
	}

	void font(HFONT font) {
		validate_hwnd();
		SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
	}

	void add_style(LONG_PTR style) {
		validate_hwnd();
		LONG_PTR currentStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
		SetWindowLongPtr(hwnd, GWL_STYLE, currentStyle | style);
	}
	void remove_style(LONG_PTR style) {
		validate_hwnd();
		LONG_PTR currentStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
		SetWindowLongPtr(hwnd, GWL_STYLE, currentStyle & ~style);
	}
	void add_style_ex(LONG_PTR styleEx) {
		validate_hwnd();
		LONG_PTR currentStyleEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, currentStyleEx | styleEx);
	}
	void remove_style_ex(LONG_PTR styleEx) {
		validate_hwnd();
		LONG_PTR currentStyleEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, currentStyleEx & ~styleEx);
	}

protected:
	void rewrite_style(LONG_PTR style) {
		validate_hwnd();
		SetWindowLongPtr(hwnd, GWL_STYLE, style);
	}
	void rewrite_style_ex(LONG_PTR styleEx) {
		validate_hwnd();
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, styleEx);
	}

public:
	// 发送消息到窗口
	LRESULT send(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const {
		validate_hwnd();
		return SendMessage(hwnd, msg, wParam, lParam);
	}

public:
	// 主消息循环
	static int run() {
		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			HWND hRootWnd = GetAncestor(msg.hwnd, GA_ROOT);
			if (!IsDialogMessage(hRootWnd, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		return static_cast<int>(msg.wParam);
	}

protected:
	virtual void onCreated() {};

private:
	// 静态消息处理函数
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		Window* pThis = nullptr;

		if (msg == WM_CREATE) {
			CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
			pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			pThis->hwnd = hwnd;
		}
		else {
			pThis = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (pThis) {
			return pThis->WndProc(msg, wParam, lParam);
		}
		else {
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	}

	// 消息处理函数
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		if (!hwnd) return 0;
		if (msg == WM_COMMAND) {
			if (process_command(wParam, lParam) == 0) {
				return 0;
			}
		}
		if (msg == WM_DESTROY) {
			hwnd = nullptr;  // 窗口被销毁时置空句柄
			if (managed.contains(hwnd)) {
				managed.erase(hwnd);
			}
			if (is_main_window) {
				PostQuitMessage(0);  // 退出消息循环
			}
		}
		if (router.contains(msg)) {
			auto& handler = router.at(msg);
			return handler(wParam, lParam);
		}
		if (msg == WM_NOTIFY) {
			return process_notification(wParam, lParam);
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	unordered_map<UINT, std::function<LRESULT(WPARAM, LPARAM)>> router;
	unordered_map<UINT, std::function<LRESULT(WPARAM, LPARAM)>>* notification_router;

protected:
	// 注册事件处理器
	template <typename Callable>
	void add_event_handler(UINT msg, const Callable& handler) {
		router.insert(std::make_pair(msg, (handler)));
	}
	template <typename Callable>
	void add_notification_handler(UINT msg, const Callable& handler) {
		notification_router->insert(std::make_pair(msg, (handler)));
	}

	virtual void setup_event_handlers() = 0;

private:
	LRESULT process_notification(WPARAM wParam, LPARAM lParam) {
		NMHDR* pNMHDR = (NMHDR*)lParam;
		if (!pNMHDR) return DefWindowProc(hwnd, WM_NOTIFY, wParam, lParam);

		HWND hWndFrom = pNMHDR->hwndFrom;
		if (managed.contains(hWndFrom)) {
			Window* target = managed.at(hWndFrom);
			auto target_router = target->notification_router;
			if (target_router->contains(pNMHDR->code)) {
				auto& handler = target_router->at(pNMHDR->code);
				return handler(wParam, lParam);
			}
		}

		return DefWindowProc(hwnd, WM_NOTIFY, wParam, lParam);
	}
	LRESULT process_command(WPARAM wParam, LPARAM lParam) {
		HWND hWndFrom = (HWND)(ULONG_PTR)lParam;
		if (managed.contains(hWndFrom)) {
			Window* target = managed.at(hWndFrom);
			auto target_router = target->notification_router;
			if (target_router->contains(HIWORD(wParam))) {
				auto& handler = target_router->at(HIWORD(wParam));
				return handler(wParam, lParam);
			}
		}

		return 1;
	}
};

#pragma region macros to simplify the event handling
#define WINDOW_EVENT_HANDLER_DECLARE_BEGIN() void setup_event_handlers() override {
#define WINDOW_EVENT_HANDLER_DECLARE_END() }

// Not necessary to super
#define WINDOW_EVENT_HANDLER_SUPER(base_class) base_class::setup_event_handlers();

#define WINDOW_add_handler(msg,handler) add_event_handler(msg, [this](WPARAM wParam, LPARAM lParam) { return handler(wParam, lParam); });
#define WINDOW_add_notification_handler(msg,handler) add_notification_handler(msg, [this](WPARAM wParam, LPARAM lParam) { return handler(wParam, lParam); });

//#pragma warning(disable: VCR001) // 未找到"WINDOW_add_handler"的函数定义。
#pragma endregion


#pragma region My Foundation Classes

class BaseSystemWindow : public Window {
public:
	BaseSystemWindow(HWND parent, const std::wstring& title, int width, int height, int x = 0, int y = 0, LONG style = WS_OVERLAPPED, LONG styleEx = 0)
		: Window(title, width, height, x, y, style, styleEx), parent(parent) {
	}
protected:
	HWND parent;
	bool class_registered() const override {
		return true;
	}
	HWND new_window() override {
		auto cls = get_class_name();
		return CreateWindowExW(
			setup_info->styleEx,
			cls.c_str(),
			setup_info->title.c_str(),
			setup_info->style,
			setup_info->x, setup_info->y,
			setup_info->width, setup_info->height,
			parent, // 必须提供，否则会失败（逆天Windows控件库。。。）并且不可以变化，否则丢消息。。。
			nullptr, nullptr, nullptr
		);
	}
	// 注意，对已经注册的Win32控件类，无法使用RegisterClassExW
	// 也就是说，我们的WndProc将不会被调用
	// 因此只能使用WINDOW_add_notification_handler而不是WINDOW_add_handler
	WINDOW_EVENT_HANDLER_DECLARE_BEGIN()
	WINDOW_EVENT_HANDLER_DECLARE_END()
};

package foundation declare;

class Static : public BaseSystemWindow {
public:
	Static(HWND parent, const std::wstring& text, int width, int height, int x = 0, int y = 0)
		: BaseSystemWindow(parent, text, width, height, x, y, WS_CHILD | WS_VISIBLE) {
	}
	~Static() override {}
protected:
	const wstring get_class_name() const override {
		return L"Static";
	}
private:
	WINDOW_EVENT_HANDLER_DECLARE_BEGIN()
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
	WINDOW_EVENT_HANDLER_DECLARE_END()
};

class Edit : public BaseSystemWindow {
public:
	Edit(HWND parent, const std::wstring& text, int width, int height, int x = 0, int y = 0)
		: BaseSystemWindow(parent, text, width, height, x, y, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL) {
	}
	~Edit() override {}
	void onChange(std::function<LRESULT(Edit*)> handler) {
		onChangeHandler = handler;
	}
	void undo() {
		validate_hwnd();
		Edit_Undo(hwnd);
	}
	void redo() {
		undo(); // win32控件的迷惑设计。。。参考：https://learn.microsoft.com/zh-cn/windows/win32/controls/em-undo
	}
	void max_length(int length) {
		validate_hwnd();
		if (length < 0) throw std::invalid_argument("Length must be greater than 0");
		Edit_LimitText(hwnd, length);
	}
	TCHAR password_char() {
		validate_hwnd();
		return Edit_GetPasswordChar(hwnd);
	}
	void password_char(TCHAR ch) {
		validate_hwnd();
		Edit_SetPasswordChar(hwnd, ch);
	}
	size_t line_count() {
		validate_hwnd();
		return Edit_GetLineCount(hwnd);
	}
	wstring get_line(int line) {
		validate_hwnd();
		wchar_t* buffer = new wchar_t[16384];
		Edit_GetLine(hwnd, line, buffer, 16384);
		wstring text(buffer);
		delete[] buffer;
		return text;
	}
	bool readonly() {
		validate_hwnd();
		return is_readonly;
	}
	void readonly(bool readonly = true) {
		validate_hwnd();
		Edit_SetReadOnly(hwnd, readonly);
		is_readonly = readonly;
	}
protected:
	const wstring get_class_name() const override {
		return L"Edit";
	}
private:
	bool is_readonly = false;
private:
	std::function<LRESULT(Edit*)> onChangeHandler;
	LRESULT onEditChanged(WPARAM wParam, LPARAM lParam) {
		if (onChangeHandler) {
			return onChangeHandler(this);
		}
		return 0;
	}

	WINDOW_EVENT_HANDLER_DECLARE_BEGIN()
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
		WINDOW_add_notification_handler(EN_CHANGE, onEditChanged);
	WINDOW_EVENT_HANDLER_DECLARE_END()
};

class Button : public BaseSystemWindow {
public:
	Button(HWND parent, const std::wstring& text, int width, int height, int x = 0, int y = 0)
		: BaseSystemWindow(parent, text, width, height, x, y, WS_CHILD | BS_CENTER | BS_DEFPUSHBUTTON | WS_VISIBLE | WS_TABSTOP) {
	}
	~Button() override {}
	void onClick(std::function<LRESULT(Button*)> handler) {
		onClickHandler = handler;
	}
protected:
	const wstring get_class_name() const override {
		return L"Button";
	}
private:
	std::function<LRESULT(Button*)> onClickHandler;
	LRESULT onBtnClicked(WPARAM wParam, LPARAM lParam) {
		if (onClickHandler) {
			return onClickHandler(this);
		}
		return 0;
	}

	// for Win32 controls, we use the notification instead of the event
	WINDOW_EVENT_HANDLER_DECLARE_BEGIN()
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
		WINDOW_add_notification_handler(BN_CLICKED, onBtnClicked);
	WINDOW_EVENT_HANDLER_DECLARE_END()
};

endpackage;
#pragma endregion



endpackage;

#undef package
#undef declare
#undef endpackage
#undef declare_exception


