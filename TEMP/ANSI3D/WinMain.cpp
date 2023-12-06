#include "Window.h"

int CALLBACK WinMain(
	HINSTANCE	hInstance,
	HINSTANCE	hPrevInstance, 
	LPSTR		lpCmdLine,
	int			nCmdShow)
{
	try {
		Window wnd(800, 300, "그러나 시간이 지나도");

		// message pump
		MSG msg;
		BOOL gResult; // int
		while ((gResult = GetMessage(&msg, nullptr, 0, 0)) > 0) {
			TranslateMessage(&msg); // Doesn't Modify Original MSG, but generates auxilary msg in certain cases
			DispatchMessage(&msg);
		}

		if (gResult == -1) {
			return -1;
		}

		return msg.wParam;
	}
	catch (const ANSIException& e) {
		MessageBox(nullptr, e.what(), e.GetType(), MB_OK | MB_ICONEXCLAMATION);
	}
	catch (const std::exception& e) {
		MessageBox(nullptr, e.what(), "Standard Exception", MB_OK | MB_ICONEXCLAMATION);
	}
	catch (...) {
		MessageBox(nullptr, "No details availbable", "Unknown Exception", MB_OK | MB_ICONEXCLAMATION);
	}
	return -1;
}