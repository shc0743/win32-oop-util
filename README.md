# win32-oop-util

**Important**: This project has been moved to [w32oop](https://github.com/shc0743/w32oop). The `win32-oop-util` has been archived.

## Usage

1. Include the `Window.hpp` in your project and add `Window.cpp`
2. Extend the w32oop::Window class

## What to do?

**Override the following function**

```cpp
virtual void setup_event_handlers() override {
	WINDOW_add_handler(WM_YourMessage, onYourMessage);
}
void onYourMessage(EventData& event) {
	// Your code here
	...
	// You can prevent the default handler
	// Default handler is DefWindowProc
	event.preventDefault();
}
// **Optional**: if you want to customize the window class
// We recommend not to override it manually
// since the framework use RTTI to generate
// the window class automatically
const wstring get_class_name() const override {
	return L"MyWndClass";
}
```

**Creating controls**
```cpp
using namespace w32oop::foundation;
class YourWindowClass : public w32oop::Window {
public:
	YourWindowClass() : Window(L"Your Window Class", 400, 300, 0, 0, WS_OVERLAPPEDWINDOW) {}
private:
	Static text;
	Button btn;
	Edit textBox;
	void onCreated() override {
		// lifecycle hooks
		text.set_parent(this); // Must call
		text.create(L"...", 250, 30, 10, 90);

		btn.set_parent(this); // Must call
		btn.create(L"Click me!", 100, 30, 10, 10);
		btn.onClick([this] (EventData& event) {
			text.text(L"Hello, World!");
		});

		textBox.set_parent(this); // Must call
		textBox.create(L"Hello, World!", 200, 30, 10, 50);
		textBox.onChange([this] (EventData& event) {
			text.text(L"Text area edited");
		});
	}
	void onDestroy() override {
		// lifecycle hooks
		// you can do your cleanup here
	}
protected:
    virtual void setup_event_handlers() override {
        
    }
};

int main() { 
	YourWindowClass window;
    window.create();
    window.set_main_window();
    window.center();
    window.show();
    return window.run();
}
```
[Full Demo Here](./examples/10.creating-controls/demo.cpp)

## More examples
[Examples](./examples/)

# License
MIT
