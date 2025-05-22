#pragma once
/*
MIT License, Copyright (c) 2025 @chcs1013
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef __cplusplus
#error "Must be included in C++"
#endif
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

#ifdef _MSVC_LANG
#if !(_MSVC_LANG > 201703L)
#error "Window.hpp needs C++20 or later."
#endif
#elif defined __cplusplus
#if !(__cplusplus > 201703L)
#error "Window.hpp needs C++20 or later."
#endif
#else
#error "No C++ supported"
#endif


namespace w32oop::util { std::wstring s2ws(const std::string str); }

package w32oop declare;

using package std;

constexpr long version = 50602000; // 5.6.2.0

declare_exception(window_not_initialized);
declare_exception(window_already_initialized);
declare_exception(window_class_registration_failure);
declare_exception(window_creation_failure);
declare_exception(window_has_no_parent);

class Window;

class EventData final {
public:
	EventData() {
		hwnd = 0;
		message = 0;
        wParam = 0;
        lParam = 0;
		bubble = false;
		isTrusted = false;
		isPreventedDefault = false;
		isStoppedPropagation = false;
		isNotification = false;
	};
	~EventData() = default;
public:
	HWND hwnd;
	UINT message; 
	WPARAM wParam;
	LPARAM lParam;
	bool bubble;
private:
	bool isTrusted;
public:
	function<void(LRESULT)> returnValue;
	function<void()> preventDefault;
	function<void()> stopPropagation;
private:
	LRESULT result;
	bool isPreventedDefault;
    bool isStoppedPropagation;
	bool isNotification;
public:
	bool is_notification() const {
		return isNotification;
	}
public:
	friend class Window;
};


constexpr UINT WINDOW_NOTIFICATION_CODES = WM_USER + 0x1011caf;
constexpr UINT WM_MENU_CHECKED = WM_USER + WM_MENUCOMMAND + 0xFFFFFF;


class Window {
private:
	static unordered_map<HWND, Window*> managed; // Internal -- DO NOT access it
	static mutex default_font_mutex;
	static HFONT default_font;
	enum GlobalOptions {
		Option_Unknown = 0,
		Option_DebugMode,
		Option_DisableDialogWindowHandling,
		Option_DisableAcceleratorHandling,
		Option_DisableHotkey,
		Option_HACCEL,
	};
	static map<GlobalOptions, long long> global_options;
protected:
	HWND hwnd = nullptr; // 窗口句柄
	
public:
	static inline void set_global_option(GlobalOptions option, long long value) {
		global_options[option] = value;
	}
	static inline long long get_global_option(GlobalOptions option) {
		if (global_options.contains(option)) {
			return global_options[option];
		}
		return 0;
	}

public:
	virtual const wstring get_class_name() const;

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

public:
	static void set_default_font(HFONT font);
	static void set_default_font(wstring font_name = L"Consolas");

private:
	wstring class_name;
	virtual void register_class_if_needed() final;

protected:
	// 检查窗口句柄有效性
	inline void validate_hwnd() const {
		if (!hwnd) throw window_not_initialized_exception();
	}

	virtual HWND new_window();

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
	Window(
		const std::wstring& title,
		int width,
		int height,
		int x = 0,
		int y = 0,
		LONG style = WS_OVERLAPPED,
		LONG styleEx = WS_EX_CONTROLPARENT
	);
	virtual ~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	Window(Window&& other) noexcept :
		hwnd(other.hwnd)
		,setup_info(other.setup_info)
		//,notification_router(other.notification_router)
	{
		other.hwnd = nullptr;
		//other.notification_router = nullptr;
		if (hwnd) {
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			managed[hwnd] = this;
		}
	}
	Window& operator=(Window&& other) noexcept;

	virtual operator HWND() const final {
		return hwnd;
	}

public:
	virtual void create() final;

	// 添加子窗口（类似appendChild）
	virtual void append(const Window& child) {
		validate_hwnd();
		SetParent(child.hwnd, hwnd);
	}

	virtual Window& parent() final {
        validate_hwnd();
		HWND parent = GetParent(hwnd);
		if (managed.contains(parent)) return *(managed.at(parent));
		throw window_has_no_parent_exception();
	}

	// 窗口操作方法
	inline void move(int x, int y) {
		validate_hwnd();
		SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	inline void resize(int w, int h) {
		validate_hwnd();
		SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	virtual void center();
	static void center(HWND, HWND parent = NULL);

	inline void set_topmost(bool isTopmost) {
		validate_hwnd();
		SetWindowPos(hwnd, isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	inline void enable(bool enabled = true) {
		validate_hwnd();
		EnableWindow(hwnd, enabled);
	}
	inline void disable() { enable(false); }

	virtual void close();

	// 窗口显示系列函数
	inline void show(int nCmdShow = SW_SHOW) {
		validate_hwnd();
		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);
	}
	inline void hide() {
		show(SW_HIDE);
	}
	inline void minimize() {
		show(SW_MINIMIZE);
	}
	inline void maximize() {
		show(SW_MAXIMIZE);
	}

	virtual void set_main_window(bool isMainWindow = true) final;

	virtual HMENU sysmenu() const;

	virtual wstring text() const;
	virtual void text(const std::wstring& text);

	virtual HFONT font() const;
	virtual void font(HFONT font);

	virtual void add_style(LONG_PTR style) final;
	virtual void remove_style(LONG_PTR style) final;
	virtual void add_style_ex(LONG_PTR styleEx) final;
	virtual void remove_style_ex(LONG_PTR styleEx) final;

protected:
	virtual void destroy() {
		validate_hwnd();
		DestroyWindow(hwnd);
	}

	virtual void override_style(LONG_PTR style) final;
	virtual void override_style_ex(LONG_PTR styleEx) final;

public:
	// 发送消息到窗口
	virtual LRESULT send(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const;
	virtual LRESULT dispatchEvent(EventData data) final;
	// 简化的事件模型，只有冒泡（bubble）模式，不支持捕获（capture）模式
private:
	virtual LRESULT dispatchEvent(EventData& data, bool isTrusted, bool shouldBubble) final;
	virtual void dispatchEventForWindow(EventData& data) final;
public:
	// 主消息循环
	static int run();

protected:
	virtual void onCreated();
	virtual void onDestroy();

private:
	// 静态消息处理函数
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	// 消息处理函数
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT dispatchMessageToWindowAndGetResult(Window& window, UINT msg, WPARAM wParam, LPARAM lParam, bool isNotification = false);

	using EventRouter = unordered_map<UINT,
		std::vector<
			std::function<void(EventData&)>
		>
	>;
	EventRouter router;
#if 0
	EventRouter* notification_router;
#endif

protected:
	// 注册事件处理器
#if 0
	template <typename Callable>
	void add_event_handler(UINT msg, const Callable &handler) {
		if (!router.contains(msg)) {
			// 如果消息不存在，创建一个新的消息处理函数列表
			router.insert(std::make_pair(msg, std::vector<Callable>()));
		}
		router->at(msg).push_back((handler));
	}
	template <typename Callable>
	void add_notification_handler(UINT msg, const Callable& handler) {
		if (!notification_router->contains(msg)) {
			// 如果消息不存在，创建一个新的消息处理函数列表
			notification_router->insert(std::make_pair(msg, std::vector<Callable>()));	
		}
		notification_router->at(msg).push_back(handler);
	}
#else
	void addEventListener(UINT msg, const function<void(EventData&)>& handler) {
		if (!router.contains(msg)) {
			// 如果消息不存在，创建一个新的消息处理函数列表
			UINT msg2  = msg;
			router.insert(std::make_pair<UINT, vector<function<void(EventData&)>>>(std::move(msg2), std::vector<function<void(EventData&)>>()));
		}
		router.at(msg).push_back((handler));
	}
	void removeEventListener(UINT msg) {
		if (!router.contains(msg)) return;
		// 清除指定的消息处理函数列表
		router.erase(msg);
	}
	template <typename Callable>
	void removeEventListener(UINT msg, const Callable& handler) {
		if (!router.contains(msg)) return;
		auto& handlers = router.at(msg);
		// 查找匹配的 handler
		auto it = std::find_if(handlers.begin(), handlers.end(),
			[&handler](const auto& func) {
				// 比较两个 std::function 是否指向相同的可调用对象
				return func.target_type() == handler.target_type() &&
					func.target<void(EventData&)>() == handler.target<void(EventData&)>();
			});
		if (it == handlers.end()) return; // 没找到
		if (handlers.size() == 1) {
			removeEventListener(msg); // 如果只有一个处理器，直接清空消息处理函数列表
			return;
		}
		// 如果有多个处理器，移除指定的处理器
		handlers.erase(it);
	}
#endif

	virtual void setup_event_handlers() = 0;

private:
#if 0
	LRESULT process_notification(WPARAM wParam, LPARAM lParam);
	LRESULT process_command(WPARAM wParam, LPARAM lParam);
#else
#endif
};

#pragma region macros to simplify the event handling
// DEPRECATED!! This macro makes code confusing and causes VCR001 Warning.
// Please directly *override* the setup_event_handlers
// virtual void setup_event_handlers() override
#define WINDOW_EVENT_HANDLER_DECLARE_BEGIN() virtual void setup_event_handlers() override {
// DEPRECATED!! This macro makes code confusing.
// Please directly }
#define WINDOW_EVENT_HANDLER_DECLARE_END() }

// Not necessary to super IF you DIRECTLY inherits the Window
// If your parent class DO SOMETHING in the setup_event_handlers, you will need to super
#define WINDOW_EVENT_HANDLER_SUPER(base_class) base_class::setup_event_handlers();

#define WINDOW_add_handler(msg,handler) addEventListener(msg, [this](EventData& data) { if (data.hwnd != this->hwnd) return;handler(data); });
//#define WINDOW_add_notification_handler(msg,handler) add_notification_handler(msg, [this](WPARAM wParam, LPARAM lParam) { return handler(wParam, lParam); });
#define WINDOW_add_notification_handler(msg,handler) addEventListener((::w32oop::WINDOW_NOTIFICATION_CODES) + (msg), [this](EventData& data) { if (data.hwnd != this->hwnd || (!data.is_notification())) return;handler(data); });
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
	HWND new_window() override;
	// 注意，对已经注册的Win32控件类，无法使用RegisterClassExW
	// 也就是说，我们的WndProc将不会被调用
	// 因此只能使用WINDOW_add_notification_handler而不是WINDOW_add_handler
	virtual void setup_event_handlers() override {}
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
	virtual void setup_event_handlers() override {
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
	}
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
	void onEditChanged(EventData& data) {
		if (onChangeHandler) {
			data.returnValue(onChangeHandler(this));
		}
	}

	virtual void setup_event_handlers() override {
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
		WINDOW_add_notification_handler(EN_CHANGE, onEditChanged);
	}
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
	void onBtnClicked(EventData& data) {
		if (onClickHandler) {
			data.returnValue(onClickHandler(this));
		}
	}

	// for Win32 controls, we use the notification instead of the event
	virtual void setup_event_handlers() override {
		WINDOW_EVENT_HANDLER_SUPER(BaseSystemWindow);
		WINDOW_add_notification_handler(BN_CLICKED, onBtnClicked);
	}
};

endpackage;
#pragma endregion



endpackage;

#undef package
#undef declare
#undef endpackage
#undef declare_exception


