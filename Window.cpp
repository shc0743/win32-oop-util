﻿#include "Window.hpp"
using namespace w32oop;
wstring w32oop::util::s2ws(const string str) {
	wstring result;
	size_t len = MultiByteToWideChar(CP_ACP, 0, str.c_str(),
		(int)(str.size()), NULL, 0);
	if (len < 0) return result;
	wchar_t* buffer = new wchar_t[len + 1];
	if (buffer == NULL) return result;
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)(str.size()),
		buffer, (int)len);
	buffer[len] = '\0';
	result.append(buffer);
	delete[] buffer;
	return result;
}


unordered_map<HWND, Window*> Window::managed;
map<Window::GlobalOptions, long long> Window::global_options;
HFONT Window::default_font;
std::recursive_mutex Window::default_font_mutex;
map<Window::HotKeyOptions, function<void(Window::HotKeyProcData&)>> Window::hotkey_handlers;
std::recursive_mutex Window::hotkey_handlers_mutex;
HHOOK Window::_hHook;
DWORD Window::message_loop_thread_id;


const wstring Window::get_class_name() const
{
	auto& type = typeid(*this);
	return util::s2ws(
		type.name() + "#(C++ Window):"s +
#ifdef _MSVC_LANG
		type.raw_name()
#else
		"C++"
#endif
		+ "#Window"
	);
}

void Window::set_default_font(HFONT font) {
	default_font_mutex.lock();
	if (default_font) DeleteObject(default_font);
	default_font = font;
	default_font_mutex.unlock();
}

void Window::set_default_font(wstring font_name) {
	default_font_mutex.lock();
	if (default_font) DeleteObject(default_font);
	default_font = CreateFontW(-14, -7, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
		OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, FF_DONTCARE,
		font_name.c_str());
	default_font_mutex.unlock();
}

void Window::register_class_if_needed() {
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
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.hbrBackground = CreateSolidBrush(get_window_background_color());
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = class_name.c_str();

		if (!RegisterClassExW(&wcex)) {
			throw window_class_registration_failure_exception();
		}
	}
}

HWND Window::new_window() {
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

Window::Window(const std::wstring& title, int width, int height, int x, int y, LONG style, LONG styleEx) {
	setup_info = new setup_info_class();
	setup_info->title = title;
	setup_info->width = width;
	setup_info->height = height;
	setup_info->x = x; setup_info->y = y;
	setup_info->style = style;
	setup_info->styleEx = styleEx;

	//notification_router = new unordered_map<UINT, std::function<LRESULT(WPARAM, LPARAM)>>();
}

Window& Window::operator=(Window&& other) noexcept {
	// 检查自赋值
	if (this != &other) {
		// 转移所有权
		hwnd = other.hwnd;
		_created = other._created;
		setup_info = other.setup_info;
		//notification_router = other.notification_router;

		// 重置源对象
		other.hwnd = nullptr;
		other._created = false;
		other.setup_info = nullptr;
		//other.notification_router = nullptr;

		// 更新窗口的用户数据指针
		if (hwnd) {
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			managed[hwnd] = this;
		}
	}
	return *this;
}

void Window::create() {
	if (_created) throw window_already_initialized_exception();
	transfer_ownership();
	try {
		setup_event_handlers();
	}
	catch (std::exception& exc) {
		throw exc;
	}
	class_name = get_class_name();
	register_class_if_needed();
	hwnd = new_window();
	if (!hwnd) {
		throw window_creation_failure_exception();
	}
	try {
		delete setup_info;
		setup_info = nullptr;
		managed[hwnd] = this;
		SendMessageW(hwnd, WM_SETFONT, (WPARAM)get_font(), 0);
		onCreated();
		_created = true;
	}
	catch (std::exception& exc) {
		DestroyWindow(hwnd);
		hwnd = nullptr;
		if (get_global_option(Option_DebugMode)) {
			fprintf(stderr, "Cannot create window!! %s\n", exc.what());
		}
		throw exc;
	}
}

void Window::center() {
	center(hwnd, GetParent(hwnd));
}
void Window::center(HWND hwnd, HWND parent) {
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
	}
	else {
		rect.left = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
		rect.top = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	}
	// 移动窗口到指定的位置
	SetWindowPos(hwnd, HWND_TOP, rect.left, rect.top, 1, 1, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

void Window::close() {
	validate_hwnd();
	send(WM_CLOSE);
}

void Window::set_main_window(bool isMainWindow) {
	validate_hwnd();
	is_main_window = isMainWindow;
}

HMENU Window::sysmenu() const {
	validate_hwnd();
	return GetSystemMenu(hwnd, FALSE);
}

wstring Window::text() const {
	validate_hwnd();
	LRESULT len = SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
	wchar_t* buffer = new wchar_t[len + 1];
	SendMessageW(hwnd, WM_GETTEXT, len + 1, (LPARAM)buffer);
	wstring text(buffer);
	delete[] buffer;
	return text;
}

void Window::text(const std::wstring& text) {
	validate_hwnd();
	SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)text.c_str());
}

HFONT Window::font() const {
	return reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
}

void Window::font(HFONT font) {
	validate_hwnd();
	SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

void Window::add_style(LONG_PTR style) {
	validate_hwnd();
	LONG_PTR currentStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
	SetWindowLongPtr(hwnd, GWL_STYLE, currentStyle | style);
}

void Window::remove_style(LONG_PTR style) {
	validate_hwnd();
	LONG_PTR currentStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
	SetWindowLongPtr(hwnd, GWL_STYLE, currentStyle & ~style);
}

void Window::add_style_ex(LONG_PTR styleEx) {
	validate_hwnd();
	LONG_PTR currentStyleEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, currentStyleEx | styleEx);
}

void Window::remove_style_ex(LONG_PTR styleEx) {
	validate_hwnd();
	LONG_PTR currentStyleEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, currentStyleEx & ~styleEx);
}

void Window::override_style(LONG_PTR style) {
	validate_hwnd();
	SetWindowLongPtr(hwnd, GWL_STYLE, style);
}

void Window::override_style_ex(LONG_PTR styleEx) {
	validate_hwnd();
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, styleEx);
}

LRESULT Window::send(UINT msg, WPARAM wParam, LPARAM lParam) const {
	validate_hwnd();
	return SendMessage(hwnd, msg, wParam, lParam);
}

BOOL Window::post(UINT msg, WPARAM wParam, LPARAM lParam) const {
	validate_hwnd();
	return PostMessage(hwnd, msg, wParam, lParam);
}

LRESULT Window::dispatchEvent(EventData data) {
	return dispatchEvent(data, false, true);
}

LRESULT Window::dispatchEvent(EventData& data, bool isTrusted, bool shouldBubble) {
	data.isTrusted = isTrusted;
	shouldBubble = shouldBubble && data.bubble;
	dispatchEventForWindow(data);
	if (!data.isStoppedPropagation) {
		if (shouldBubble) try {
			return parent().dispatchEvent(data);
		}
		catch (window_has_no_parent_exception&) {}
	}
	if (!data.isPreventedDefault && !data.isNotification) {
		data.result = DefWindowProcW(data.hwnd, data.message, data.wParam, data.lParam);
	}
	return data.result;
}

void Window::dispatchEventForWindow(EventData& data) {
	if (!router.contains(data.message)) return;
	try {
		auto& handlers = router.at(data.message);
		for (auto& handler : handlers) {
			try {
				handler(data);
				if (data.isStoppedPropagation) break;
			}
			catch (std::exception& e) {
				if (get_global_option(Option_DebugMode)) {
					string what = "[ERROR] Unexpected exception in event handler: ";
					what += e.what();
					fwrite(what.c_str(), sizeof(decltype(what)::value_type), what.size(), stderr);
					DebugBreak();
				}
				throw;
			}
		}
	}
	catch (out_of_range&) {
		// 消息路由被外部修改了。这种情况应该不会出现
		return;
	}
	catch (...) {
		// 其他异常
		throw;
	}
}

Window::~Window() {
	if (setup_info) {
		delete setup_info;
		setup_info = nullptr;
	}
	if (!hwnd) return;
	if (managed.contains(hwnd)) {
		managed.erase(hwnd);
	}
	destroy();
}

namespace w32oop {
	class windowRunHookManager {
	public:
		HHOOK hHook;
        windowRunHookManager() : hHook(nullptr) {}
		~windowRunHookManager() {
			if (hHook) UnhookWindowsHookEx(hHook);
		}
	};
}

int Window::run() {
	MSG* lpMsg = new MSG;
	windowRunHookManager hookManager;
	message_loop_thread_id = GetCurrentThreadId();
	try {
		bool dialogHandling = !get_global_option(Option_DisableDialogWindowHandling);
		bool acceleratorHandling = !get_global_option(Option_DisableAcceleratorHandling);
		HACCEL acceleratorTable = reinterpret_cast<HACCEL>(get_global_option(Option_HACCEL));
		if (!acceleratorTable) acceleratorHandling = false;

		// 设置hook
		if (get_global_option(Option_EnableHotkey) || get_global_option(Option_EnableGlobalHotkey)) {
			DWORD dwThreadId = (get_global_option(Option_EnableGlobalHotkey) ? 0 : GetCurrentThreadId());
			int idHook = (get_global_option(Option_EnableGlobalHotkey) ? WH_KEYBOARD_LL : WH_KEYBOARD);
			HOOKPROC proc = (get_global_option(Option_EnableGlobalHotkey) ? keyboard_proc_LL : keyboard_proc);
			hookManager.hHook = SetWindowsHookExW(
				idHook, proc, GetModuleHandleW(NULL), dwThreadId
			);
			if (!hookManager.hHook) {
				if (get_global_option(Option_DebugMode)) {
					fprintf(stderr, "[Window] SetWindowsHookExW failed: %d\n", GetLastError());
					DebugBreak();
				}
			}
			_hHook = hookManager.hHook;
		}

		HWND hRootWnd = NULL;
		while (GetMessageW(lpMsg, nullptr, 0, 0)) {
			if (dialogHandling || acceleratorHandling) hRootWnd = GetAncestor(lpMsg->hwnd, GA_ROOT);
			if (dialogHandling) {
				if (IsDialogMessageW(hRootWnd, lpMsg)) continue;
			}
			if (acceleratorHandling) {
				if (TranslateAcceleratorW(hRootWnd, acceleratorTable, lpMsg)) continue;
			}
			// normal message handling
			TranslateMessage(lpMsg);
			DispatchMessageW(lpMsg);
		}
		int returnValue = static_cast<int>(lpMsg->wParam);
		delete lpMsg;
		return returnValue;
	}
	catch (const std::exception&) {
		delete lpMsg;
		throw;
	}
}

LRESULT Window::keyboard_proc(
	_In_ int    code,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
) {
	lock_guard lock(hotkey_handlers_mutex);
	// 如果 代码 小于零，挂钩过程必须将消息传递给 CallNextHookEx 函数，而无需进一步处理，并且应返回 CallNextHookEx 返回的值。
	// https://learn.microsoft.com/zh-cn/windows/win32/winmsg/keyboardproc
	if (code < 0) {
		return CallNextHookEx(_hHook, code, wParam, lParam);
	}
	int key = (int)wParam;
	bool
		ctrl = GetAsyncKeyState(VK_CONTROL) & 0x8000,
		shift = GetAsyncKeyState(VK_SHIFT) & 0x8000,
		alt = ((lParam & (static_cast<long long>(1) << 29)) != 0);
	for (auto& pair : hotkey_handlers) {
		if (pair.first.ctrl == ctrl && pair.first.shift == shift && pair.first.alt == alt && pair.first.vk == key) {
			HotKeyProcData data;
			bool prevented = false;
			data.preventDefault = [&prevented]() {
				prevented = true;
			};
			data.wParam = wParam;
			data.lParam = lParam;
			data.pKbdStruct = nullptr;
			data.source = pair.first.source;
			pair.second(data);
			if (prevented) return 1;
			return CallNextHookEx(_hHook, code, wParam, lParam);
		}
	}
	return CallNextHookEx(_hHook, code, wParam, lParam);
}

LRESULT Window::keyboard_proc_LL(
	_In_ int    code,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
) {
	lock_guard lock(hotkey_handlers_mutex);
	// 如果 代码 小于零，挂钩过程必须将消息传递给 CallNextHookEx 函数，而无需进一步处理，并且应返回 CallNextHookEx 返回的值。
	// https://learn.microsoft.com/zh-cn/windows/win32/winmsg/keyboardproc
	if ((code < 0) || (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN)) {
		return CallNextHookEx(_hHook, code, wParam, lParam);
	}
	PKBDLLHOOKSTRUCT p = reinterpret_cast<PKBDLLHOOKSTRUCT>(lParam);
	int vk = p->vkCode;
	bool 
		ctrl = GetAsyncKeyState(VK_CONTROL) & 0x8000,
        shift = GetAsyncKeyState(VK_SHIFT) & 0x8000,
        alt = GetAsyncKeyState(VK_MENU) & 0x8000;
	for (auto& pair : hotkey_handlers) {
		if (pair.first.ctrl == ctrl && pair.first.shift == shift && pair.first.alt == alt && pair.first.vk == vk) {
			if (pair.first.scope == HotKeyOptions::Thread) {
				HWND currentWindow = GetForegroundWindow();
				if (!currentWindow) continue;
				DWORD tid = GetWindowThreadProcessId(currentWindow, NULL);
				if (tid != message_loop_thread_id) continue;
			}
			HotKeyProcData data;
			bool prevented = false;
			data.preventDefault = [&prevented]() {
				prevented = true;
			};
			data.wParam = wParam;
			data.lParam = lParam;
			data.pKbdStruct = p;
			data.source = pair.first.source;
			pair.second(data);
			if (prevented) return 1;
			return CallNextHookEx(_hHook, code, wParam, lParam);
		}
	}
	return CallNextHookEx(_hHook, code, wParam, lParam);
}

void Window::onCreated() {}
void Window::onDestroy() {}

LRESULT Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

LRESULT Window::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (!hwnd) return 0;
	if (msg == WM_DESTROY) {
		// 窗口被销毁
		return destroy_handler_internal(msg, wParam, lParam);
	}
	// 处理窗口消息
	// 首先判断特殊的窗口消息，检查源窗口到底是哪个
	if (msg == WM_COMMAND || msg == WM_NOTIFY) {
		HWND targetWindow = NULL;
		WPARAM notifCode = 0;
		if (msg == WM_COMMAND) {
			auto hi = HIWORD(wParam), lo = LOWORD(wParam);
			if (lParam == 0) {
				// “菜单”或者“加速器”（也就是快捷键）消息
				auto id = lo;
				return dispatchMessageToWindowAndGetResult(WM_MENU_CHECKED, id, 0);
			}
			// 来自控件的通知代码。
			// 此时消息实际上并不是我们的窗口，而是控件的窗口。
			// 我们需要将消息转发给对应的窗口进行处理。
            targetWindow = (HWND)lParam;
			notifCode = hi;
		}
        if (msg == WM_NOTIFY) {
			NMHDR* ptrhdr = (NMHDR*)lParam;
			// 检查内存地址是否有效（这是为了提升程序的安全性）。
			NMHDR hdr{};
			bool success = ReadProcessMemory(GetCurrentProcess(), ptrhdr, &hdr, sizeof(NMHDR), nullptr);
			if (!success) {
				// the message is evil!!
				if (get_global_option(Option_DebugMode)) {
					string message = "The message is evil!! at " + to_string((ULONGLONG)(void*)hwnd) +
						", received WM_NOTIFY, lParam = " + to_string(lParam);
					fwrite(message.c_str(), message.size(), 1, stderr);
				}
				return DefWindowProc(hwnd, msg, wParam, lParam);
			}
			targetWindow = hdr.hwndFrom;
			//notifCode = hdr.idFrom;
			notifCode = hdr.code;
        }
		try {
			Window* target = managed.at(targetWindow);
			return target->dispatchMessageToWindowAndGetResult(UINT(notifCode + WINDOW_NOTIFICATION_CODES), (UINT)msg, lParam, true);
		}
		catch (std::out_of_range&) {
			// 找不到对应的窗口
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	}
	// 现在源窗口应该就是我们的窗口了。
	// 直接处理
	return dispatchMessageToWindowAndGetResult(msg, wParam, lParam, false);
}

LRESULT Window::dispatchMessageToWindowAndGetResult(UINT msg, WPARAM wParam, LPARAM lParam, bool isNotification) {
	// 构造EventData
	EventData data;
	data.hwnd = hwnd;
    data.message = msg;
    data.wParam = wParam;
    data.lParam = lParam;
	data.isNotification = isNotification;
	data.bubble = data.isNotification; // 只有通知消息才冒泡，否则会出现问题
	data._source = this;
	
	// 设置处理程序
	data.returnValue = [&](LRESULT value) {
		data.result = value;
		// 自动 preventDefault
		data.preventDefault();
	};
	data.stopPropagation = [&]() {
		data.isStoppedPropagation = true;
	};
    data.preventDefault = [&]() {
		data.isPreventedDefault = true;
	};

	// 分发消息
	return dispatchEvent(data, true, data.bubble);
}

LRESULT Window::destroy_handler_internal(UINT msg, WPARAM wParam, LPARAM lParam) {
	// cleanups
	onDestroy();
	if (is_main_window) {
		// 确保正确退出
		PostQuitMessage(0);
	}
	auto result = DefWindowProc(hwnd, msg, wParam, lParam);
	// 必须清理钩子！
	remove_all_hot_key_on_window();
	// 清理 managed
	if (managed.contains(hwnd)) {
		managed.erase(hwnd);
	}
	hwnd = nullptr;
	return result;
}


bool Window::hotkey_handler_contains(bool ctrl, bool shift, bool alt, int vk_code, HotKeyOptions::Scope scope) {
	lock_guard lock(hotkey_handlers_mutex);
	for (auto& pair : hotkey_handlers) {
		auto& item = pair.first;
        if (item.ctrl == ctrl && item.alt == alt && item.shift == shift && item.vk == vk_code && item.scope == scope) {
			return true;
		}
	}
    return false;
}

void Window::register_hot_key(
	bool ctrl, bool alt, bool shift, int vk_code,
	function<void(HotKeyProcData&)> callback, HotKeyOptions::Scope scope
) {
	if (!get_global_option(Option_EnableHotkey))
		set_global_option(Option_EnableHotkey, true);
	if (scope == HotKeyOptions::Scope::System) {
		if (!get_global_option(Option_EnableGlobalHotkey))
            set_global_option(Option_EnableGlobalHotkey, true);
	}

	HotKeyOptions options;
    options.ctrl = ctrl;
    options.alt = alt;
    options.shift = shift;
    options.vk = vk_code;
    options.scope = scope;
	options.source = this;

	lock_guard lock(hotkey_handlers_mutex);
	if (hotkey_handler_contains(ctrl, alt, shift, vk_code, scope)) {
		throw window_hotkey_duplication_exception();
	}
    hotkey_handlers.insert(make_pair(options, callback));
}

void Window::remove_hot_key(bool ctrl, bool alt, bool shift, int vk_code, HotKeyOptions::Scope scope) {
	lock_guard lock(hotkey_handlers_mutex);
	if (!hotkey_handler_contains(ctrl, alt, shift, vk_code, scope)) {
		return;
	}
	for (auto& pair : hotkey_handlers) {
		if (pair.first.ctrl == ctrl && pair.first.alt == alt && pair.first.shift == shift && pair.first.vk == vk_code && pair.first.scope == scope) {
            hotkey_handlers.erase(pair.first);
			break;
		}
	}
}

void Window::remove_all_hot_key_on_window() {
	lock_guard lock(hotkey_handlers_mutex);
	auto it = hotkey_handlers.begin();
	while (it != hotkey_handlers.end()) {
		if (it->first.source == this) {
			// 安全地移除元素并获取下一个有效的迭代器
			it = hotkey_handlers.erase(it);
		}
		else {
			++it;
		}
	}
}

void Window::remove_all_hot_key_global() {
	lock_guard lock(hotkey_handlers_mutex);
	// 直接清空
    hotkey_handlers.clear();
}


#pragma region My Foundation Classes

HWND BaseSystemWindow::new_window() {
	auto cls = get_class_name();
	return CreateWindowExW(
		setup_info->styleEx,
		cls.c_str(),
		setup_info->title.c_str(),
		setup_info->style,
		setup_info->x, setup_info->y,
		setup_info->width, setup_info->height,
		parent, // 必须提供，否则会失败（逆天Windows控件库。。。）并且不可以变化，否则丢消息。。。
		(HMENU)(LONG_PTR)(ctlid), nullptr, nullptr
	);
}

#pragma endregion

const char* version_string() {
	return "w32oop::version_string 5.6.4.4 (C++ Win32 Object-Oriented Programming Framework) GI/5.6";
}
