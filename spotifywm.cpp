#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>		/* to declare xEvent */
#include <xcb/xproto.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#define STR_(x) # x
#define STR(x)  STR_(x)

extern "C" {
	extern char * program_invocation_short_name; // provided by glibc
}

void spotifywm_init(void) __attribute__((constructor));
void spotifywm_init(void) {
	// Prevent spotifywm.so from being attached to processes started by spotify
	const char *envname = "LD_PRELOAD";
	const char *oldenv = getenv(envname);
	if (oldenv && false) {
		char *env = strdup(oldenv);
		char *pos = strstr(env, STR(SONAME));
		if (pos) {
			size_t len1 = strlen(STR(SONAME));
			size_t len2 = strlen(pos + len1);
			memmove(pos, pos + len1, len2);
			*(pos + len2) = '\0';
			setenv(envname, env, 1);
		}
		free(env);
	}
	fprintf(stderr, "[spotifywm] attached to spotify\n");
}

#define BASE_NAME(SymbolName) base_ ## SymbolName
#define TYPE_NAME(SymbolName) SymbolName ## _t
#define INTERCEPT(ReturnType, SymbolName, ...) \
		typedef ReturnType (*TYPE_NAME(SymbolName))(__VA_ARGS__); \
	static void * const BASE_NAME(SymbolName) = dlsym(RTLD_NEXT, STR(SymbolName)); \
	extern "C" ReturnType SymbolName(__VA_ARGS__)
#define BASE(SymbolName) ((TYPE_NAME(SymbolName))BASE_NAME(SymbolName))

INTERCEPT(int, XMapWindow,
	Display * dpy,
	Window    w
) {
	XClassHint* classHint;

	fprintf(stderr, "[spotifywm] spotify window %lx found\n", w);
	classHint = XAllocClassHint();
	if (classHint) {
		classHint->res_name = (char*)"spotify";
		classHint->res_class = (char*)"Spotify";
		XSetClassHint(dpy, w, classHint);
		XFree(classHint);
	}
	return BASE(XMapWindow)(dpy, w);
}

INTERCEPT(unsigned int, xcb_send_request,
	xcb_connection_t *c,
	int flags,
	struct iovec *vector,
	const xcb_protocol_request_t *request 
) {
	// Check if we are sending a MapWindow request
	// This code is called from chromium's source code:
	// ui/gfx/x/generated_protos/xproto.cc in XProto::MapWindow
	if(request->count >= 1 && vector[0].iov_len >= 8 && ((uint8_t*)vector[0].iov_base)[0] == 8) {
		uint32_t window_id = ((uint32_t*)vector[0].iov_base)[1];

		fprintf(stderr, "[spotifywm] spotify window %x found\n", window_id);
		
		// Don't use the same XCB connection as spotify is checking the returned number
		Display* dpy = XOpenDisplay(NULL);
		XClassHint* classHint = XAllocClassHint();
		if (classHint) {
			classHint->res_name = (char*)"spotify";
			classHint->res_class = (char*)"Spotify";
			XSetClassHint(dpy, window_id, classHint);
			XFree(classHint);
		}
		XCloseDisplay(dpy);
	}
	
	return BASE(xcb_send_request)(c, flags, vector, request);
}
