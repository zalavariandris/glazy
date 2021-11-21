#pragma once
#include "Stylus.h"
#include <iostream>
#include <windows.h>

/**
* IMPLEMENTATION
*/
HHOOK _hook;
Pen pen;

std::map<UINT32, Touch> touches;
int touch_count_change{ 0 };
float touch_delta_x;
float touch_delta_y;
double touch_zoom_delta{ 1.0 };

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0)
	{
		LPMSG pStruct = (LPMSG)lParam;
		UINT message = pStruct->message;

		switch (message) {
		case WM_POINTERDOWN:
		case WM_POINTERUPDATE:
		case WM_POINTERUP:
			POINTER_INFO pointerInfo = {};
			// Get frame id from current message
			if (GetPointerInfo(GET_POINTERID_WPARAM(pStruct->wParam), &pointerInfo)) {
				POINT p = pointerInfo.ptPixelLocation;

				// native touch to screen conversion, alt use ofGetWindowPosition offsets
				ScreenToClient(pStruct->hwnd, &p);

				auto x = p.x;
				auto y = p.y;
				auto pointer_id = pointerInfo.pointerId;
				POINTER_INPUT_TYPE input_type = pointerInfo.pointerType;

				//std::cout << "type: ";
				switch (input_type) {
				case PT_POINTER:
				{
					//std::cout << "pointer" << std::endl;
				}
				break;
				case PT_TOUCH:
				{
					POINTER_TOUCH_INFO touch_info;
					GetPointerTouchInfo(pointer_id, &touch_info);
					if (pointerInfo.pointerFlags & POINTER_FLAG_DOWN) {
						touches[pointer_id] = Touch();

						touches[pointer_id].x = p.x;
						touches[pointer_id].y = p.y;
					}

					if (pointerInfo.pointerFlags & POINTER_FLAG_UPDATE) {
						touches[pointer_id].x = p.x;
						touches[pointer_id].y = p.y;
					}

					else if (pointerInfo.pointerFlags & POINTER_FLAG_UP) {
						auto it = touches.find(pointer_id);
						if (it != touches.end()) {
							touches.erase(it);
						}
					}

				}
				break;
				case PT_PEN:
				{
					POINTER_PEN_INFO pen_info;
					GetPointerPenInfo(pointer_id, &pen_info);

					static int last_inrange = -1;
					bool IN_RANGE = pointerInfo.pointerFlags & POINTER_FLAG_INRANGE;
					if (IN_RANGE != last_inrange) {
						if (IN_RANGE)
							pen.inRange = true;
						else
							pen.inRange = false;

						last_inrange = IN_RANGE;
					}

					if (pointerInfo.pointerFlags & POINTER_FLAG_DOWN) {
						pen.isDown = true;

					}
					else if (pointerInfo.pointerFlags & POINTER_FLAG_UPDATE) {
						pen.x = p.x;
						pen.y = p.y;

						pen.pressure = (double)pen_info.pressure / 1024;
						pen.tilt_x = pen_info.tiltX;
						pen.tilt_y = pen_info.tiltY;
						pen.rotation = pen_info.rotation;
					}
					else if (pointerInfo.pointerFlags & POINTER_FLAG_UP) {
						pen.isDown = false;
					}

					//std::cout << "pen" << std::endl;
					//std::cout << "- pos: " << p.x << "," << p.y << std::endl;
					//std::cout << "- rotation: " << pen_info.rotation << std::endl;
					//std::cout << "- tilt x: " << pen_info.tiltX << std::endl;
					//std::cout << "- tilt y: " << pen_info.tiltY << std::endl;
					//std::cout << "- pressure: " << pen_info.pressure << std::endl;
					//std::cout << std::endl;
				}
				break;
				case PT_MOUSE:
					std::cout << "mouse" << std::endl;
					break;
				case PT_TOUCHPAD:
					std::cout << "touchpad" << std::endl;
					break;
				}
			}

			break;
		}
	}
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

void Stylus::Init() {
	//auto hwnd = glfwGetWin32Window(glazy::window);
	EnableMouseInPointer(FALSE);
	int windowsHookCode = WH_GETMESSAGE;

	if (!(_hook = SetWindowsHookEx(windowsHookCode, HookCallback, GetModuleHandle(NULL), GetCurrentThreadId()))) {
		//MessageBox(NULL, "Failed to install hook!", "Error", MB_ICONERROR);
		std::cout << "Failed to install hook!" << std::endl;
	}
}

void Stylus::NewFrame() {
	static std::map<UINT32, Touch> last_touches{ touches };

	// sum all touch delta 
	touch_delta_x = 0;
	touch_delta_y = 0;

	for (auto it = touches.begin(); it != touches.end(); it++)
	{
		auto touch_id = it->first;
		auto last_touch_it = last_touches.find(touch_id);
		if (last_touch_it != last_touches.end()) // had touch
		{
			auto last_touch = last_touch_it->second;
			touches[touch_id].deltax = touches[touch_id].x - last_touch.x;
			touches[touch_id].deltay = touches[touch_id].y - last_touch.y;

			touch_delta_x += touches[touch_id].deltax;
			touch_delta_y += touches[touch_id].deltay;
		}
	}

	touch_delta_x /= touches.size();
	touch_delta_y /= touches.size();

	// calc zoom delta
	touch_zoom_delta = 1.0;
	if (touches.size() == 2 && last_touches.size() == 2) {
		auto touch1 = touches.begin()->second;
		auto touch2 = (--touches.end())->second;
		auto distance = sqrt(pow(touch1.x - touch2.x, 2) + pow(touch1.y - touch2.y, 2));

		auto last_touch1 = last_touches.begin()->second;
		auto last_touch2 = (--last_touches.end())->second;
		auto last_distance = sqrt(pow(last_touch1.x - last_touch2.x, 2) + pow(last_touch1.y - last_touch2.y, 2));

		touch_zoom_delta = distance / last_distance;
	}

	// keep touch count change
	touch_count_change = touches.size() - last_touches.size();

	// keep current touches for next frame
	last_touches = touches;
}

bool Stylus::IsPenInRange() {
	return pen.inRange;
}

bool Stylus::IsPenDown() {
	return pen.isDown;
}

double Stylus::GetPenPressure() {
	if (!pen.isDown) {
		return 1.0;
	}
	return pen.pressure;
}

bool Stylus::TouchDown(int touch_count) {
	if (touch_count_change > 0) {
		if (touches.size() >= touch_count) {
			return true;
		}
	}
	return false;
}

bool Stylus::TouchUp(int touch_count) {
	if (touch_count_change < 0) {
		if (touches.size() - touch_count_change >= touch_count) {
			return true;
		}
	}
	return false;
}

bool Stylus::TouchDragging(int touch_count) {
	if (touches.size() >= touch_count) {
		if (touch_delta_x != 0 || touch_delta_y != 0) {
			return true;
		}
	}
	return false;
}

bool Stylus::IsAnyTouchDown() {
	return touches.size() > 0;
}

bool Stylus::IsTouchPanning(float lock_threshold) {
	return false;
}

bool Stylus::IsTouchZooming(float lock_threshold) {
	return false;
}

bool Stylus::IsTouchRotating(float lock_threshold) {
	return false;
}

double Stylus::GetTouchDeltaX() {
	return touch_delta_x;
}

double Stylus::GetTouchDeltaY() {
	return touch_delta_y;
}

double Stylus::GetTouchZoomDelta() {
	return touch_zoom_delta;
}

const std::map<unsigned int, Touch> Stylus::GetTouches() {
	return touches;
}