# win32-oop-util

Win32 OOP Utility

## Usage

1. Include the `Window.hpp` in your project and add `Window.cpp`
2. Extend the w32oop::Window class

## What to do?

**Override the following function**

```cpp
	const wstring get_class_name() const override {
		return L"MyWndClass";
	}
```


