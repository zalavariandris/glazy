#pragma once

#include <map>

struct Pen {
	bool isDown{ false };
	bool inRange{ false };

	double x{ 0.0 };
	double y{ 0.0 };

	double pressure{ 1.0 };
	unsigned int rotation{ 0 };
	unsigned int tilt_x{ 0 };
	unsigned int tilt_y{ 0 };
};

struct Touch {
	bool isPrimary{ false };
	bool isDown{ false };
	double x{ 0.0 };
	double y{ 0.0 };
	double deltax{ 0.0 };
	double deltay{ 0.0 };
};

namespace Stylus {
	// pen
	bool IsPenInRange();
	bool IsPenDown();
	double GetPenPressure();

	// touch
	const std::map<unsigned int, Touch> GetTouches();
	bool TouchDown(int touch_count = 0);
	bool TouchUp(int touch_count = 0);
	bool TouchDragging(int touch_count = 0);
	bool IsAnyTouchDown();
	bool IsTouchPanning(float lock_threshold = 0.0);
	bool IsTouchZooming(float lock_threshold = 0.0);
	bool IsTouchRotating(float lock_threshold = 0.0);
	double GetTouchDeltaX();
	double GetTouchDeltaY();
	double GetTouchZoomDelta();

	void Init();

	void NewFrame();

	inline void Destroy() {}
}