// compile command: cl /EHsc demo.cpp /std:c++20
#define UNICODE 1
#define _UNICODE 1
#include "Window.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace w32oop;
using namespace w32oop::foundation;

class MyApp : public Window {
protected:
	const wstring get_class_name() const override final {
		return L"myAppClass";
	}
protected:
    Button* myButton=0;

public:
	MyApp(const wstring& title, int width, int height, int x = 0, int y = 0)
		: Window(title, width, height, x, y, WS_OVERLAPPEDWINDOW)
    {
        // Do not initialize the button here
    }
    ~MyApp() override {
        if (myButton) delete myButton;
    }
protected:
    void onCreated() override {
        // Instead, create the button here
        myButton = new Button(*this, L"Click me!", 60, 30, 10, 10);
        // although looks ugly, we have to call the create
        myButton->create();
        // set event handler
        myButton->onClick([this](Button* btn) {
            MessageBoxW(hwnd, L"Button clicked!", L"Notification", MB_OK);
            return 0;
        });
    }
	LRESULT onClicked(WPARAM wParam, LPARAM lParam) {
		MessageBoxW(hwnd, L"Window clicked!", L"Notification", MB_OK);
		return 0;
	}
private:
	WINDOW_EVENT_HANDLER_DECLARE_BEGIN()
		WINDOW_add_handler(WM_LBUTTONDOWN, onClicked);
	WINDOW_EVENT_HANDLER_DECLARE_END()
};

int main2() {
    // create the application
    MyApp app(L"My Application", 640, 480);
    // create it
    app.create();
    // set the main window
    app.set_main_window();
    // center
    app.center();
    // show it
    app.show();
    // message loop
    return Window::run();
}

int main() { 
    try {
        int ret = main2();
        return ret;
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}