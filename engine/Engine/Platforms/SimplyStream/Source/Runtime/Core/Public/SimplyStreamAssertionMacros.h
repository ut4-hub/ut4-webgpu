// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <emscripten.h>

#if DO_CHECK

// For the asm.js builds, use emscripten-specific versions of these macros

#define checkCode(...)
#define checkNoReentry(...)
#define checkNoRecursion(...)

void html5_break_msg(const char* msg, const char* file, int line);
void checkNoEntry_internal(const char* file, int line);
#define checkNoEntry() { EM_ASM({ console.error("WNDR CHECKFAIL checkNoEntry() @ " + UTF8ToString($0) + ":" + $1); }, __FILE__, __LINE__); checkNoEntry_internal(__FILE__, __LINE__); }
#define check(expr)			{ if (!(expr)) { \
		EM_ASM({ console.error("WNDR CHECKFAIL " + UTF8ToString($0) + " @ " + UTF8ToString($1) + ":" + $2); }, #expr, __FILE__, __LINE__); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}
//emscripten_log(255, ##__VA_ARGS__);
#define checkf(expr, ...)	{ if (!(expr)) { \
		EM_ASM({ console.error("WNDR CHECKFAIL(f) " + UTF8ToString($0) + " @ " + UTF8ToString($1) + ":" + $2); }, #expr, __FILE__, __LINE__); \
		FDebug::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
	CA_ASSUME(expr); \
}
#define verify(expr)		{ if(!(expr)) {\
		EM_ASM({ console.error("WNDR CHECKFAIL(v) " + UTF8ToString($0) + " @ " + UTF8ToString($1) + ":" + $2); }, #expr, __FILE__, __LINE__); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}
#define verifyf(expr, ...)	{ if(!(expr)) { \
		EM_ASM({ console.error("WNDR CHECKFAIL(vf) " + UTF8ToString($0) + " @ " + UTF8ToString($1) + ":" + $2); }, #expr, __FILE__, __LINE__); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}

#endif
