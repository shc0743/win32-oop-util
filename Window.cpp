#include "Window.hpp"
namespace w32oop::util {
	wstring s2ws(const string str) {
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
}
using namespace w32oop;
unordered_map<HWND, Window*> Window::managed;
HFONT Window::default_font;
std::mutex Window::default_font_mutex;