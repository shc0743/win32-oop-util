// compile command: cl /EHsc demo.cpp /std:c++20
#define UNICODE 1
#define _UNICODE 1
#include "../../Window.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(linker, "/entry:mainCRTStartup /subsystem:windows")

using namespace w32oop;
using namespace w32oop::foundation;

namespace MyDemo {

    class SimpleAppDemo : public Window {
    protected:
        //const wstring get_class_name() const override final {
        //	return L"myAppClass";
        //}
    protected:
        Button* myButton;
        Edit* myTextBox;
        Static* myResult;

    public:
        SimpleAppDemo(const wstring& title, int width, int height, int x = 0, int y = 0)
            : Window(title, width, height, x, y, WS_OVERLAPPEDWINDOW)
        {
            // Do not initialize the button here
        }
        ~SimpleAppDemo() override {
            if (myButton) delete myButton;
            if (myTextBox) delete myTextBox;
            if (myResult) delete myResult;
        }
    protected:
        void onCreated() override {
            // Instead, create the button here
            myButton = new Button(*this, L"Click me!", 80, 30, 10, 10);
            // although looks ugly, we have to call the create
            myButton->create();
            // set event handler
            myButton->onClick([this](EventData& event) {
                MessageBoxW(hwnd, (
                    L"You typed:" + myTextBox->text()).c_str(), L"Notification", MB_OK);
            });
            // create the edit box
            myTextBox = new Edit(*this, L"Type here", 200, 30, 100, 10);
            myTextBox->create();
            myTextBox->onChange([this](EventData& event) {
                // Get the text from the edit box
                wstring text = dynamic_cast<Edit&>(*event.source()).text();
                // or directly:
                // wstring text = event.source()->text(); // using polymorphism
                // Set the text to the static control
                myResult->text(text);
            });
            // create the static control
            myResult = new Static(*this, L"Result", 200, 30, 100, 50);
            myResult->create();
        }
        void onClicked(EventData&) {
            MessageBoxW(hwnd, L"Window clicked!", L"Notification", MB_OK);
        }
    private:
        virtual void setup_event_handlers() override {
            WINDOW_add_handler(WM_LBUTTONDOWN, onClicked);
        }
    };

    int main2() {
        // create the application
        SimpleAppDemo app(L"My Application", 640, 480);
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
}

int main() { 
    try {
        int ret = MyDemo::main2();
        return ret;
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}