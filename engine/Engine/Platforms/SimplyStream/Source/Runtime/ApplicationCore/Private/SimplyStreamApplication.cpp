// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "../Public/SimplyStreamApplication.h"
#include "../../../ThirdParty/wasm_utils/wndr_probe.h"
#include "../Public/SimplyStreamCursor.h"
#include "../Public/SimplyStreamInputInterface.h"
#include "HAL/OutputDevices.h"
#include "Async/Async.h"
#include "Async/AsyncWork.h"
#include "utf8.h"
#include <vector>
#include <thread>
#include <mutex>
#include "wasm_utils.h"
// TODO: move mouse and keyboard handlers to SimplyStreamInputInterface.cpp

static pthread_t ue4ThreadId = 0;

DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamApplication, Log, All);

#include <emscripten/emscripten.h>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif
#include <emscripten/key_codes.h>
#include <emscripten/html5.h>
#include "SimplyStreamJavaScriptFx.h"
//#include "archive.h"
#include "AtomicQueue.h"
#include "wonder_log.h"
template <typename T>
using HeapMethodQueue = atomic_queue::AtomicQueueB2<T, std::allocator<T>, true, true, false>;
template <typename T, uint32_t SIZE>
using StackMethodQueue = atomic_queue::AtomicQueue2<T, SIZE, true, true, true, false>;

//#define EM_ASM(...)
//#define EM_ASM_INT(...) false
static const uint32 MaxWarmUpTicks = 10;
FSimplyStreamApplication* SimplyStreamApp = 0;
ICursor* SimplyStreamCursor = 0;
void SystemResolutionSinkCallback();
extern int canvas_w, canvas_h;
extern int canvas_w2, canvas_h2;
extern int canvas_css_w, canvas_css_h;
bool pointerlockIsActive = false;
bool has_focus = false;
extern bool runningNode;
bool ue_desires_pointer_lock = false;

bool SetUEDesiresPointerLock(bool desires_pointer_lock) {
	if (ue_desires_pointer_lock == desires_pointer_lock) return false;
	ue_desires_pointer_lock = desires_pointer_lock;
	//wonder_log::info("setting ue_desires_pointer_lock = {}", desires_pointer_lock);
	return true;
}
bool GetUEDesiresPointerLock() {
	static auto last_pointer_lock = ue_desires_pointer_lock;
	if (last_pointer_lock != ue_desires_pointer_lock) {
		last_pointer_lock = ue_desires_pointer_lock;
		//wonder_log::info("returning ue_desires_pointer_lock = {}", ue_desires_pointer_lock);
	}
	return ue_desires_pointer_lock;
}
EM_BOOL mouse_move_callback2(int evType, const EmscriptenMouseEvent* evt, void* userData) {
	/* rescale (in case canvas is being scaled)*/
	double xscale, yscale;
	xscale = canvas_w2 / double(canvas_css_w);
	yscale = canvas_h2 / double(canvas_css_h);
	int calc_x = (int)(evt->targetX * xscale + .5);
	int calc_y = (int)(evt->targetY * yscale + .5);
	//wonder_log::info("MouseMoveCB Pos({} {} {} {}) XRel: {} YRel: {} {} {} {} {}", (int)evt->targetX, calc_x, (int)evt->targetY, calc_y, (int)evt->movementX, (int)evt->movementY, canvas_w2, canvas_h2, canvas_css_w, canvas_css_h);
	//SimplyStreamCursor->SetPosition(evt->movementX, calc_y);
	SimplyStreamCursor->SetPosition(evt->targetX, evt->targetY);
	return 0;
}
FSimplyStreamApplication* FSimplyStreamApplication::CreateSimplyStreamApplication()
{
	return new FSimplyStreamApplication();
}

KEEP_IN_MODULE void downloadTrace3();

bool FSimplyStreamApplication::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("printFSStats"))) {
		/*UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
		FConsoleOutputDevice StrOut(ViewportConsole);
		auto v0 = _fs.file_mem_in_use.load();
		uint64_t B2 = v0 * 100;
		auto KB2 = B2 >> 10;
		auto MB2 = KB2 >> 10;
		float v1 = float(MB2) / 100;
		StrOut.Logf(TEXT("Filesystem Memory Usage: %f MB"), v1);
		return true;*/
	}
	else if (FParse::Command(&Cmd, TEXT("showFSStats"))) {
		/*UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
		FConsoleOutputDevice StrOut(ViewportConsole);
		auto v0 = _fs.file_mem_in_use.load();
		uint64_t B2 = v0 * 100;
		auto KB2 = B2 >> 10;
		auto MB2 = KB2 >> 10;
		float v1 = float(MB2) / 100;
		StrOut.Logf(TEXT("Filesystem Memory Usage: %f MB"), v1);
		return true;*/
	}
	else if (FParse::Command(&Cmd, TEXT("downloadTrace"))) {
#if UE_TRACE_ENABLED
		//UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
		//FConsoleOutputDevice StrOut(ViewportConsole);
		//auto v0 = _fs.file_mem_in_use.load();
		//uint64_t B2 = v0 * 100;
		//auto KB2 = B2 >> 10;
		//auto MB2 = KB2 >> 10;
		//float v1 = float(MB2) / 100;
		//StrOut.Logf(TEXT("Downloading trace file."));
		downloadTrace3();
		return true;
#else
		//UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
		//FConsoleOutputDevice StrOut(ViewportConsole);
		//StrOut.Logf(TEXT("Downloading trace file failed. Compile with Unreal Insights Support."));
		wonder_log::error("Downloading trace file failed. Compile with Unreal Insights Support.");
		return false;
#endif
	}
	return false;
}

#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_DEFAULT 0
#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO 1
#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES 2
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT 3
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO 4
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES 5
extern __attribute__((used, visibility("default"))) bool canvas_size_changed;
extern float canvas_multiplier;
APPLICATIONCORE_API void viewportUpdate(int width, int height);
extern "C" __attribute__((used, visibility("default"))) void viewportUpdate3(int width, int height, int cssWidth, int cssHeight);

EM_BOOL pointerlockchange_callback(int eventType, const EmscriptenPointerlockChangeEvent *pointerLockChangeEvent, void *userData)
{
	FSimplyStreamApplication *application = reinterpret_cast<FSimplyStreamApplication*>(userData);
	if (!application) return 0;
	application->OnPointerLockChangeEvent(eventType, pointerLockChangeEvent);
	return 0;
}
extern EM_BOOL captureResizeEvent3(int eventType, const EmscriptenUiEvent* e, void* rawState) {
	viewportUpdate3(canvas_w, canvas_h, canvas_css_w, canvas_css_h);
	return 1;
};
void SimplyStreamFocusCallback_mainthread(int eventType, int eventType2) {
	if (!SimplyStreamApp) return;
	SimplyStreamApp->OnFocusEvent(eventType, nullptr);
}

static EM_BOOL key_callback_ue4_thread(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
	FSimplyStreamApplication *application = reinterpret_cast<FSimplyStreamApplication*>(userData);
	if (!application) return 0;
	return application->OnKeyEvent(eventType, keyEvent);
}
static EM_BOOL mouse_callback_ue4_thread(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
	FSimplyStreamApplication* application = reinterpret_cast<FSimplyStreamApplication*>(userData);
	if (!application) return 0;
	mouse_move_callback2(eventType, mouseEvent, userData);
	return application->OnMouseEvent(eventType, mouseEvent);
}
static EM_BOOL wheel_callback_ue4_thread(int eventType, const EmscriptenWheelEvent* wheelEvent, void* userData)
{
	FSimplyStreamApplication* application = reinterpret_cast<FSimplyStreamApplication*>(userData);
	if (!application) return 0;
	return application->OnWheelEvent(eventType, wheelEvent);
}
void SimplyStreamHandleLMouseDown_mainthread(const int32_t x, const int32_t y) {
	SimplyStreamApp->SimplyStreamHandleLMouseDown(x, y);
}
void SimplyStreamHandleRMouseDown_mainthread(const int32_t x, const int32_t y) {
	SimplyStreamApp->SimplyStreamHandleRMouseDown(x, y);
}
void SimplyStreamHandleLMouseUp_mainthread(const int32_t x, const int32_t y) {
	SimplyStreamApp->SimplyStreamHandleLMouseUp(x, y);
}
void SimplyStreamHandleRMouseUp_mainthread(const int32_t x, const int32_t y) {
	SimplyStreamApp->SimplyStreamHandleRMouseUp(x, y);
}
void SimplyStreamHandleKeyChar_mainthread(const int32_t ActualKey, const bool bIsRepeat) {
	SimplyStreamApp->SimplyStreamHandleKeyChar(ActualKey, bIsRepeat);
}
void SimplyStreamHandleKeyDown_mainthread(const int32_t ActualKey, const uint32_t CharCode, const bool bIsRepeat) {
	SimplyStreamApp->SimplyStreamHandleKeyDown(ActualKey, CharCode, bIsRepeat);
}
void SimplyStreamHandleKeyUp_mainthread(const int32_t ActualKey, const uint32_t CharCode) {
	SimplyStreamApp->SimplyStreamHandleKeyUp(ActualKey, CharCode);
}
void SimplyStreamHandleMousePos_mainthread(const int32_t PosX, const int32_t PosY, const int32_t DeltaX, const int32_t DeltaY) {
	SimplyStreamApp->SimplyStreamHandleMousePos(PosX, PosY, DeltaX, DeltaY);
}
void SimplyStreamHandleActivate_mainthread() {
	SimplyStreamApp->SimplyStreamHandleActivate();
}
void SimplyStreamHandleKeyChar(const int32_t ActualKey, const bool bIsRepeat) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleKeyChar_mainthread, 0, ActualKey, bIsRepeat);
}
void SimplyStreamHandleKeyDown(const int32_t ActualKey, const uint32_t CharCode, const bool bIsRepeat) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VIII, SimplyStreamHandleKeyDown_mainthread, 0, ActualKey, CharCode, bIsRepeat);
}
void SimplyStreamHandleKeyUp(const int32_t ActualKey, const uint32_t CharCode) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleKeyUp_mainthread, 0, ActualKey, CharCode);
}
void SimplyStreamHandleMousePos(const int32_t PosX, const int32_t PosY, const int32_t DeltaX, const int32_t DeltaY) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VIIII, SimplyStreamHandleMousePos_mainthread, 0, PosX, PosY, DeltaX, DeltaY);
}
void SimplyStreamHandleLMouseDown(const int32_t x, const int32_t y) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleLMouseDown_mainthread, 0, x, y);
}
void SimplyStreamHandleRMouseDown(const int32_t x, const int32_t y) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleRMouseDown_mainthread, 0, x, y);
}
void SimplyStreamHandleActivate() {
	if (!SimplyStreamApp) return;
	static bool in_focus = false;
	if (!in_focus) {
		in_focus = true;
	}
	else {
		return;
	}
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_V, SimplyStreamHandleActivate_mainthread, 0);
}
void SimplyStreamHandleLMouseUp(const int32_t x, const int32_t y) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleLMouseUp_mainthread, 0, x, y);
}
void SimplyStreamHandleRMouseUp(const int32_t x, const int32_t y) {
	if (!SimplyStreamApp) return;
	emscripten_dispatch_to_thread(ue4ThreadId, EM_FUNC_SIG_VII, SimplyStreamHandleRMouseUp_mainthread, 0, x, y);
}
void FSimplyStreamApplication::SimplyStreamHandleKeyChar(const int32_t ActualKey, const bool bIsRepeat) {
	MessageHandler->OnKeyChar(ActualKey, bIsRepeat);
}
void FSimplyStreamApplication::SimplyStreamHandleKeyDown(const int32_t ActualKey, const uint32_t CharCode, const bool bIsRepeat) {
	MessageHandler->OnKeyDown(ActualKey, CharCode, bIsRepeat);
}
void FSimplyStreamApplication::SimplyStreamHandleKeyUp(const int32_t ActualKey, const uint32_t CharCode) {
	MessageHandler->OnKeyUp(ActualKey, CharCode, false);
}
void FSimplyStreamApplication::SimplyStreamHandleMousePos(const int32_t PosX, const int32_t PosY, const int32_t DeltaX, const int32_t DeltaY) {
	FVector2D CursorPos = /*GEngine->GameViewport->GetWindow()->GetPositionInScreen() +*/ FVector2D(PosX, PosY);
	Cursor->SetPosition(CursorPos.X, CursorPos.Y);
	MessageHandler->OnRawMouseMove(DeltaX, DeltaY);
	MessageHandler->OnMouseMove();
}
void FSimplyStreamApplication::SimplyStreamHandleLMouseDown(const int32_t x, const int32_t y) {
	MessageHandler->OnMouseDown(ApplicationWindow, EMouseButtons::Left);
}
void FSimplyStreamApplication::SimplyStreamHandleRMouseDown(const int32_t x, const int32_t y) {
	MessageHandler->OnMouseDown(ApplicationWindow, EMouseButtons::Right);
}
void FSimplyStreamApplication::SimplyStreamHandleLMouseUp(const int32_t x, const int32_t y) {
	MessageHandler->OnMouseUp(EMouseButtons::Left);
}
void FSimplyStreamApplication::SimplyStreamHandleRMouseUp(const int32_t x, const int32_t y) {
	MessageHandler->OnMouseUp(EMouseButtons::Right);
}
void FSimplyStreamApplication::SimplyStreamHandleActivate() {
	MessageHandler->OnCursorSet();
}

APPLICATIONCORE_API int processingBehaviorCheck(int, const char*, int, int, uint32_t);

enum class EmEventType {
	Keyboard,
	Mouse,
	Wheel,
	Focus,
	PointerLock,
};
struct EmEvent {
	int eventType;
	void* event;
	void* userData;
	EmEventType type;
};
//StackMethodQueue<EmEvent, 65536> event_queue;
HeapMethodQueue<EmEvent> event_queue{ 65536 };
// This function is called on the main browser thread, and not on the application main thread!
#define WNDR_INPUT_USE_PROXY 0
#define WNDR_INPUT_USE_PROXY_LEGACY 0
#define WNDR_INPUT_USE_PROXY_CUSTOM 1
void do_events() {
	EmEvent event;
	while (event_queue.try_pop(event)) {
		switch (event.type)
		{
		case EmEventType::Keyboard:
			//wonder_log::info("proxyKeyboardEvent {} {} {} {} {} {}", event.eventType, ((EmscriptenKeyboardEvent*)event.event)->key, ((EmscriptenKeyboardEvent*)event.event)->keyCode, ((EmscriptenKeyboardEvent*)event.event)->which, ((EmscriptenKeyboardEvent*)event.event)->charCode, ((EmscriptenKeyboardEvent*)event.event)->code);
			key_callback_ue4_thread(event.eventType, (EmscriptenKeyboardEvent*)event.event, event.userData);
			break;
		case EmEventType::Mouse:
			//wonder_log::info("proxyMouseEvent {} {} {} {} {} {} {} {} {}", event.eventType, ((EmscriptenMouseEvent*)event.event)->screenX, ((EmscriptenMouseEvent*)event.event)->screenY, ((EmscriptenMouseEvent*)event.event)->clientX, ((EmscriptenMouseEvent*)event.event)->clientY, ((EmscriptenMouseEvent*)event.event)->movementX, ((EmscriptenMouseEvent*)event.event)->movementY, canvas_w, canvas_h);
			mouse_callback_ue4_thread(event.eventType, (EmscriptenMouseEvent*)event.event, event.userData);
			break;
		case EmEventType::Wheel:
			wheel_callback_ue4_thread(event.eventType, (EmscriptenWheelEvent*)event.event, event.userData);
			break;
		case EmEventType::Focus:
			SimplyStreamFocusCallback_mainthread(event.eventType, 0);
			break;
		case EmEventType::PointerLock:
			pointerlockchange_callback(event.eventType, (EmscriptenPointerlockChangeEvent*)event.event, event.userData);
			break;
		default:
			break;
		}
	}
}
uint64_t mouseEventIndex = 0;
uint64_t keyEventIndex = 0;
static std::array<EmscriptenMouseEvent, 0x100> mouseEvents; // 4096 mouse events = 4096 * 64 bytes = 256KB
static std::array<EmscriptenKeyboardEvent, 0x100> keyEvents; // 4096 mouse events = 4096 * 64 bytes = 256KB
KEEP_IN_MODULE void proxyMouseEvent(int eventType, EmscriptenMouseEvent* e) {
	if (GetUEDesiresPointerLock())
	{
		if (!pointerlockIsActive)
		{
			// Pointer lock REQUIRES a user gesture: requesting it from mousemove (every
			// event while unlocked) threw an uncaught NotAllowedError PER MOUSE EVENT —
			// the exception storms in every frozen-session report. Request only from
			// down/up (gesture-qualified), and swallow the rejection: denial is the
			// expected outcome until the user actually clicks.
			if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN || eventType == EMSCRIPTEN_EVENT_MOUSEUP)
			{
				WNDR_PROBED_MAIN_THREAD_EM_ASM({
					var p = document.getElementById('canvas-container').requestPointerLock();
					if (p && p.catch) { p.catch(function(){}); }
					// Force keyboard focus on the canvas, so that any other DOM elements having active input control will no longer be accepting
					Module['canvas'].focus();
					});
			}
		}
	}
	else {
		if (!has_focus) {
			WNDR_PROBED_MAIN_THREAD_EM_ASM({ Module['canvas'].focus(); });
		}
	}
	auto index = mouseEventIndex++ % mouseEvents.size();
	auto& me = mouseEvents[index];
	memcpy(&me, e, sizeof(EmscriptenMouseEvent));
	event_queue.push(EmEvent{ eventType, &me, SimplyStreamApp, EmEventType::Mouse });
}
KEEP_IN_MODULE void proxyKeyboardEvent(int eventType, EmscriptenKeyboardEvent* e) {
	//wonder_log::info("proxyKeyboardEvent {} {} {} {} {} {}", eventType, e->key, e->keyCode, e->which, e->charCode, e->code);
	auto index = keyEventIndex++ % keyEvents.size();
	auto& ke = keyEvents[index];
	memcpy(&ke, e, sizeof(EmscriptenKeyboardEvent));
	event_queue.push(EmEvent{ eventType, &ke, SimplyStreamApp, EmEventType::Keyboard });
}
KEEP_IN_MODULE void proxyWheelEvent(int eventType, EmscriptenWheelEvent* e) {
	event_queue.push(EmEvent{ eventType, e, SimplyStreamApp, EmEventType::Wheel });
}
KEEP_IN_MODULE void proxyFocusEvent(int eventType) {
	event_queue.push(EmEvent{ eventType, 0, SimplyStreamApp, EmEventType::Focus });
}
KEEP_IN_MODULE void proxyPointerLockEvent(int eventType, EmscriptenPointerlockChangeEvent* e) {
	event_queue.push(EmEvent{ eventType, e, SimplyStreamApp, EmEventType::PointerLock });
}
EM_BOOL key_callback_browser_thread(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
	// Was a return-false stub -> keyboard never reached Slate (could not type/login).
	// Forward to the ue4 thread via the queue (proxyKeyboardEvent copies into its ring).
	// Return false (do NOT preventDefault) so the browser still emits keypress events,
	// which carry the character for text entry (OnKeyChar).
	proxyKeyboardEvent(eventType, const_cast<EmscriptenKeyboardEvent*>(e));
	return false;
}

static inline const char* emscripten_event_type_to_string(int eventType) {
	const char* events[] = { "(invalid)", "(none)", "keypress", "keydown", "keyup", "click", "mousedown", "mouseup", "dblclick", "mousemove", "wheel", "resize",
	  "scroll", "blur", "focus", "focusin", "focusout", "deviceorientation", "devicemotion", "orientationchange", "fullscreenchange", "pointerlockchange",
	  "visibilitychange", "touchstart", "touchend", "touchmove", "touchcancel", "gamepadconnected", "gamepaddisconnected", "beforeunload",
	  "batterychargingchange", "batterylevelchange", "webglcontextlost", "webglcontextrestored", "(invalid)" };
	++eventType;
	if (eventType < 0) eventType = 0;
	if (eventType >= sizeof(events) / sizeof(events[0])) eventType = sizeof(events) / sizeof(events[0]) - 1;
	return events[eventType];
}
static EM_BOOL mouse_callback_browser_thread(int eventType, const EmscriptenMouseEvent *e, void *userData) {
	// Forward to the ue4 thread via the event queue (proxyMouseEvent copies e into its
	// own ring buffer, so passing the stack event is safe). Was a return-false stub ->
	// mouse input never reached Slate. Drained by do_events() in PollGameDeviceState.
	proxyMouseEvent(eventType, const_cast<EmscriptenMouseEvent*>(e));
	return pointerlockIsActive || (!pointerlockIsActive && e->targetX >= 0 && e->targetY >= 0 && e->targetX < canvas_css_w && e->targetY < canvas_css_h);
}

static EM_BOOL wheel_callback_browser_thread(int eventType, const EmscriptenWheelEvent *e, void *userData) {
	// Let JavaScript page developer hook into mouse wheel input event handling behavior.
	int processingBehavior = EM_ASM_INT({ if (Module['UE4_wheelEvent']) { return Module['UE4_wheelEvent']($0, $1, $2, $3, $4, $5, $6, $7); }}, eventType, e->mouse.canvasX, e->mouse.canvasY, e->mouse.button, e->mouse.buttons, e->deltaX, e->deltaY, e);
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO) return EM_FALSE;
	if (processingBehavior != UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT)
	{
		// Asynchronously post the input event to UE4 main engine thread to handle.
		EmscriptenWheelEvent *wheelEvent = (EmscriptenWheelEvent*)malloc(sizeof(*e));
		memcpy(wheelEvent, e, sizeof(*e));
		proxyWheelEvent(eventType, wheelEvent);
	}
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO) return EM_FALSE;

	return pointerlockIsActive || (!pointerlockIsActive && e->mouse.canvasX >= 0 && e->mouse.canvasY >= 0 && e->mouse.canvasX < canvas_css_w && e->mouse.canvasY < canvas_css_h);
}

EM_BOOL FSimplyStreamApplication::OnFocusEvent(int eventType, const EmscriptenFocusEvent *focusEvent) {
	switch(eventType) {
	case EMSCRIPTEN_EVENT_FOCUS:
		//wonder_log::info("WindowFocusGained");
		MessageHandler->OnCursorSet();
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
		WarmUpTicks = 0;
		break;

	case EMSCRIPTEN_EVENT_BLUR:
		//wonder_log::info("WindowFocusLost");
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
		break;
	}
	return 0;
}

EM_BOOL FSimplyStreamApplication::OnPointerLockChangeEvent(int eventType, const EmscriptenPointerlockChangeEvent *pointerlockChange) {
	//wonder_log::info("PointerLockChangeEvent Active: {}", pointerlockChange->isActive);
	pointerlockIsActive = pointerlockChange->isActive;
	if (pointerlockChange->isActive) {
		MessageHandler->OnCursorSet();
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
	}
	else
	{
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
		// wndr: the browser consumes the Escape key to exit pointer lock, so UE never receives the
		// keypress and the in-game menu (bound to EKeys::Escape via the "ShowMenu" action) never opens.
		// When the lock is lost while UE still WANTED it (i.e. the user pressed Esc during captured
		// gameplay, not because we opened a menu ourselves), synthesize the Escape key so ShowMenu fires
		// and the pause menu appears. keyCode 27 == DOM_VK_ESCAPE, which UE maps to EKeys::Escape.
		if (GetUEDesiresPointerLock())
		{
			MessageHandler->OnKeyDown(27 /*DOM_VK_ESCAPE*/, 27, false);
			MessageHandler->OnKeyUp(27 /*DOM_VK_ESCAPE*/, 27, false);
		}
	}
	return EM_FALSE;
}

static int InterpretCharCode(int eventType, const EmscriptenKeyboardEvent *keyEvent) {
	if (eventType == EMSCRIPTEN_EVENT_KEYPRESS && keyEvent->which) return keyEvent->which;
	if (keyEvent->charCode) return keyEvent->charCode;
	if (strlen(keyEvent->key) == 1) return (int)tolower(keyEvent->key[0]);
	if (keyEvent->which) return keyEvent->which;
	return keyEvent->keyCode;
}

static int NumCharsInUTF8String(const unsigned char *str) {
	if (!str) return 0;
	int numChars = 0;
	while(*str)
		if ((*str++ & 0xC0) != 0x80) ++numChars; // Skip all continuation bytes
	return numChars;
}
EM_BOOL FSimplyStreamApplication::OnKeyEvent(int eventType, const EmscriptenKeyboardEvent *keyEvent) {
	// To test out keyEvent fields in action in your browser, visit
	// https://s3.amazonaws.com/mozilla-games/test_keyboard_codes/test_keyboard_codes.html
	// which is a live compiled build of https://github.com/kripken/emscripten/tree/incoming/tests/test_keyboard_codes.c
	int keyCode = keyEvent->keyCode;
	int CharCode = InterpretCharCode(eventType, keyEvent);
	static_assert(sizeof(TCHAR) == 2, "The line below assumes TCHAR=UTF16");
	std::vector<unsigned short> utf16result;
	utf8::unchecked::utf8to16((unsigned char*)keyEvent->key, (unsigned char*)keyEvent->key + 1, back_inserter(utf16result));
	TCHAR Character = (TCHAR)utf16result.front();
	int domPhysicalKeyCode = emscripten_compute_dom_pk_code(keyEvent->code);
	if (domPhysicalKeyCode == DOM_PK_BACKQUOTE)  {
		keyCode = DOM_VK_BACK_QUOTE;
		CharCode = Character = '`';
	}

	// HACK: trap (') key presses (i.e. do not process this key)
	// SimplyStream builds crash on assertion failure for some reason.
	if (!strcmp(keyEvent->key, "'")) return 0;

	// HACK: trap F5 key presses (i.e. do not process this key)
	// browser will reload page -- which can lead to unknown conditions and crashes...
	if (domPhysicalKeyCode == DOM_PK_F5) return 0;

	if (keyEvent->location == 2 && (keyCode == DOM_VK_WIN || keyCode == DOM_VK_CONTROL || keyCode == DOM_VK_SHIFT || keyCode == DOM_VK_ALT))
	{
		keyCode += 256;
	}

	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_KEYDOWN:
		//wonder_log::info("KeyDown: {}", keyEvent->key);
		MessageHandler->OnKeyDown(keyCode, CharCode, keyEvent->repeat);

		if (keyCode == DOM_VK_BACK_SPACE || keyCode == DOM_VK_RETURN || keyCode == DOM_VK_TAB)
		{
			MessageHandler->OnKeyChar(keyCode, keyEvent->repeat);
		}
		break;

	case EMSCRIPTEN_EVENT_KEYUP:
		MessageHandler->OnKeyUp(keyCode, CharCode, keyEvent->repeat);
		break;

	case EMSCRIPTEN_EVENT_KEYPRESS:
		if (NumCharsInUTF8String((unsigned char*)keyEvent->key) == 1)
		{
			MessageHandler->OnKeyChar(Character, keyEvent->repeat);
		}
		else
		{

		}
		break;
	}
	return 0;
}

// https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent/button
static EMouseButtons::Type DomMouseButtonToUE4MouseButton(int button) {
	switch(button) {
		case 0: return EMouseButtons::Left;
		case 1: return EMouseButtons::Middle;
		case 2: return EMouseButtons::Right;
		case 3: return EMouseButtons::Thumb01; // Note: Most web browsers don't really send buttons 3/4, but usually hardcode them for navigation.
		case 4: return EMouseButtons::Thumb02;
		default: return EMouseButtons::Invalid;
	}
}

EM_BOOL FSimplyStreamApplication::OnMouseEvent(int eventType, const EmscriptenMouseEvent *mouseEvent) {
	{ static uint32 _mi=0; if(_mi++<40) UE_LOG(LogTemp, Warning, TEXT("SSINPUT evt=%d btn=%d tX=%d tY=%d runningNode=%d"), eventType, (int)mouseEvent->button, (int)mouseEvent->targetX, (int)mouseEvent->targetY, (int)runningNode); }
	if (runningNode) {
		return EM_FALSE;
	}
	switch(eventType) {
	case EMSCRIPTEN_EVENT_MOUSEDOWN:
		MessageHandler->OnMouseDown(ApplicationWindow, DomMouseButtonToUE4MouseButton(mouseEvent->button));
		break;
	case EMSCRIPTEN_EVENT_MOUSEUP:
		MessageHandler->OnMouseUp(DomMouseButtonToUE4MouseButton(mouseEvent->button));
		break;
	case EMSCRIPTEN_EVENT_MOUSEMOVE: {
			bool bMouseMove = true;
#if 0
			if ( !has_focus ) {
				if (ue_desires_pointer_lock) {
					// Only send relative mouse motion events to the application when pointer lock is active, since the camera moving
					// around when one hovers the mouse cursor around the scene is distracting UX.
					if (!pointerlockIsActive) {
						bMouseMove = false;
					}
				}
			}
#endif
			if (bMouseMove) {
				if (Cursor.IsValid()) { Cursor->SetPosition((int32)mouseEvent->targetX, (int32)mouseEvent->targetY); }
				MessageHandler->OnRawMouseMove(mouseEvent->movementX, mouseEvent->movementY);
				MessageHandler->OnMouseMove();
				//wonder_log::info("has_focus: {} pointerlockIsActive: {} MouseMove cX={} cY={} mX={} mY={} ue_desires_pointer_lock={} ", has_focus, pointerlockIsActive, mouseEvent->canvasX, mouseEvent->canvasY, mouseEvent->movementX, mouseEvent->movementY, ue_desires_pointer_lock);
			}
		}
		break;
	}
	return 0;
}
EM_BOOL FSimplyStreamApplication::OnWheelEvent(int eventType, const EmscriptenWheelEvent *wheelEvent) {
	switch(eventType) {
	case EMSCRIPTEN_EVENT_WHEEL:
		const float SpinFactor = 1 / 120.0f;
		MessageHandler->OnMouseWheel(wheelEvent->deltaY * SpinFactor);
		break;
	}
	return 0;
}
KEEP_IN_MODULE void enqueueFocusEvent() {
	proxyFocusEvent(EMSCRIPTEN_EVENT_FOCUS);
}
KEEP_IN_MODULE void enqueueBlurEvent() {
	proxyFocusEvent(EMSCRIPTEN_EVENT_BLUR);
}
FSimplyStreamApplication::FSimplyStreamApplication()
	: GenericApplication( MakeShareable( new FSimplyStreamCursor() ) )
	, InputInterface(FSimplyStreamInputInterface::Create(MessageHandler, Cursor))
	, ApplicationWindow(FSimplyStreamWindow::Make())
	, WarmUpTicks(-1) {

	SimplyStreamApp = this;
	SimplyStreamCursor = Cursor.Get();
	/* Needed currently for RCS - hackish
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	MessageHandler->OnSizeChanged(ApplicationWindow, 3440, 1440, false);
	MessageHandler->OnResizingWindow(ApplicationWindow);
	BroadcastDisplayMetricsChanged(DisplayMetrics);
	SystemResolutionSinkCallback();
	MessageHandler->OnCursorSet();
	MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
	*/
	ue4ThreadId = pthread_self();

	if (runningNode) {
		EM_ASM(console.log('Not enabling local input as this is a RCS server'));
		return;
	}
	emscripten_set_keydown_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_keyup_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_keypress_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

	emscripten_set_mousedown_callback_on_thread("#canvas-container", this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_mouseup_callback_on_thread("#canvas-container", this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_mousemove_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

	emscripten_set_wheel_callback_on_thread("#canvas", this, 0, wheel_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	//emscripten_set_pointerlockchange_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, pointerlockchange_callback, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, pointerlockchange_callback);

	//emscripten_set_focus_callback_on_thread("#canvas", this, 1, focus_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD);
	//emscripten_set_blur_callback("#canvas", this, 1, blur_callback2);
	WNDR_PROBED_MAIN_THREAD_EM_ASM({
		document.getElementById('canvas').addEventListener('focus', function() {
			_enqueueFocusEvent();
		});
		document.getElementById('canvas').addEventListener('blur', function() {
			_enqueueBlurEvent();
		});
	});
	emscripten_set_resize_callback("#canvas-container", nullptr, false, captureResizeEvent3);
}
void FSimplyStreamApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) {
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}
void FSimplyStreamApplication::PollGameDeviceState( const float TimeDelta ) {
	do_events(); // drain queued browser input (mouse/keyboard/wheel) into Slate
	InputInterface->SendControllerEvents();

	if (SetUEDesiresPointerLock((((FSimplyStreamCursor*)Cursor.Get())->LockStatus && !((FSimplyStreamCursor*)Cursor.Get())->CursorStatus))) {}
	//if (SetUEDesiresPointerLock(((FSimplyStreamCursor*)Cursor.Get())->LockStatus)) {}
	//wonder_log::info("pointer LockStatus: {} CursorStatus: {}", ((FSimplyStreamCursor*)Cursor.Get())->LockStatus, ((FSimplyStreamCursor*)Cursor.Get())->CursorStatus);

	static auto last_cursor_status = ((FSimplyStreamCursor*)Cursor.Get())->CursorStatus;
	if (last_cursor_status != ((FSimplyStreamCursor*)Cursor.Get())->CursorStatus) {
		last_cursor_status = ((FSimplyStreamCursor*)Cursor.Get())->CursorStatus;
		if (!last_cursor_status) {
			WNDR_PROBED_MAIN_THREAD_EM_ASM({
				// set the cursor to none
				document.getElementById('canvas').style.cursor = 'none';
				});
		}
		else {
			WNDR_PROBED_MAIN_THREAD_EM_ASM({
				document.getElementById('canvas').style.cursor = 'auto';
				});
		}
	}
	//SetUEDesiresPointerLock(((FSimplyStreamCursor*)Cursor.Get())->LockStatus);

	static bool init = true;
	if (runningNode && init && 0) {
		init = false;
		//EM_ASM(console.log('Setting screen size'));
		viewportUpdate(3440, 1440);
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

		MessageHandler->OnSizeChanged(ApplicationWindow, 3440, 1440, false);
		MessageHandler->OnResizingWindow(ApplicationWindow);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
		SystemResolutionSinkCallback();
	}
	// cache lock status so we dont have to poll browser?
	if (!ue_desires_pointer_lock && !runningNode)
	{
		if (pointerlockIsActive)
		{
			//wonder_log::info("Exiting pointer lock {} {}", ((FSimplyStreamCursor*)Cursor.Get())->LockStatus, ((FSimplyStreamCursor*)Cursor.Get())->CursorStatus);
			WNDR_PROBED_MAIN_THREAD_EM_ASM({
				//console.log('Exiting pointer lock');
				document.exitPointerLock();
			});
			pointerlockIsActive = false;
		}
	}

	if (canvas_size_changed && !runningNode) {
		canvas_size_changed = false;

		//EM_ASM({ console.log("Canvas resized to", $0, $1) }, canvas_w, canvas_h);

		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);

		MessageHandler->OnResizingWindow(ApplicationWindow);
		MessageHandler->OnMovedWindow(ApplicationWindow, 0, 0);
		MessageHandler->OnSizeChanged(ApplicationWindow, float(canvas_w) * canvas_multiplier, float(canvas_h) * canvas_multiplier, false);
		MessageHandler->OnResizingWindow(ApplicationWindow);

		SystemResolutionSinkCallback();
	}
}
void FSimplyStreamApplication::SetCapture(const TSharedPtr< FGenericWindow >& InWindow)
{
#if 0
	if (InWindow.IsValid())
	{
		SetUEDesiresPointerLock(true);
#if 0
		pointerlockIsActive = true;
		has_focus = true;
		WNDR_PROBED_MAIN_THREAD_EM_ASM({
			document.getElementById('canvas-container').requestPointerLock();
			// Force keyboard focus on the canvas, so that any other DOM elements having active input control will no longer be accepting
			Module['canvas'].focus();
			});
#endif
	}
	else
	{
		SetUEDesiresPointerLock(false);
#if 0
		pointerlockIsActive = false;
		WNDR_PROBED_MAIN_THREAD_EM_ASM({
			console.log('Exiting pointer lock2');
			document.exitPointerLock();
			});
#endif
	}
#endif
}

FPlatformRect FSimplyStreamApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FSimplyStreamWindow::GetScreenRect();
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FSimplyStreamWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect    =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
	OutDisplayMetrics.PrimaryDisplayWidth   =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
	OutDisplayMetrics.PrimaryDisplayHeight  =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;
	//UE_LOG(LogSimplyStreamApplication, Verbose, TEXT("GetDisplayMetrics Width:%d, Height:%d"), OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right, OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom);
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< FGenericWindow > FSimplyStreamApplication::MakeWindow()
{
	return ApplicationWindow;
}
