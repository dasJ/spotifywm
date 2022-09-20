#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define STR_(x) # x
#define STR(x)  STR_(x)

extern "C" {
	extern char * program_invocation_short_name; // provided by glibc
}

void spotifywm_init(void) __attribute__((constructor));
void spotifywm_init(void) {
	// Prevent spotifywm.so from being attached to processes started by steam
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

	fprintf(stderr, "[spotifywm] spotify window found\n");
	classHint = XAllocClassHint();
	if (classHint) {
		classHint->res_name = "spotify";
		classHint->res_class = "Spotify";
		XSetClassHint(dpy, w, classHint);
		XFree(classHint);
	}
	return BASE(XMapWindow)(dpy, w);
}
