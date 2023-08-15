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
#include <unistd.h>

#include <vector>
#include <string>

#define STR_(x) # x
#define STR(x)  STR_(x)

extern "C" {
	extern char * program_invocation_short_name; // provided by glibc
}

static uint32_t X11_ATOM_NET_WM_STATE;
static uint32_t X11_ATOM_NET_WM_STATE_MAXIMIZED_VERT;
static uint32_t X11_ATOM_NET_WM_STATE_MAXIMIZED_HORZ;
static uint32_t X11_ATOM_NET_ACTIVE_WINDOW;

void spotifywm_init(void) __attribute__((constructor));
void spotifywm_init(void) {
	// Prevent spotifywm.so from being attached to processes started by steam
	const char *envname = "LD_PRELOAD";
	const char *oldenv = getenv(envname);
	if (oldenv) {
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

static void init_atoms() {
	if(X11_ATOM_NET_WM_STATE == 0) {
		Display* dpy = XOpenDisplay(":0");
		X11_ATOM_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", 1);
		X11_ATOM_NET_WM_STATE_MAXIMIZED_VERT = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", 1);
		X11_ATOM_NET_WM_STATE_MAXIMIZED_HORZ = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", 1);
		X11_ATOM_NET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", 1);
		XCloseDisplay(dpy);
	}
}

#ifdef DEBUG
static std::string get_atom_name(uint32_t atom) {
	Display* dpy = XOpenDisplay(":0");
	char* result = XGetAtomName(dpy, atom);
	std::string ret;
	if(result) {
		ret = result;
	}
	XCloseDisplay(dpy);

	return ret;
}
#endif

template<typename T>
static const T* concat_iovec_buffers(struct iovec *vector, size_t count) {
	if(count == 0) {
		return nullptr;
	}

	if(count == 1) {
		if(vector[0].iov_len >= sizeof(T))
			return (const T*) vector[0].iov_base;
		else
			return nullptr;
	}
	
	fprintf(stderr, "can't convert buffer into pointer of %zu bytes as it uses %zu iovec buffers\n", sizeof(T), count);
	
	return nullptr;
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
	struct __attribute__((packed)) RequestHeader {
		uint8_t major_opcode;
		uint8_t minor_opcode;
		uint16_t length;
		uint32_t data[0];
	};
	struct RequestHeader* header;
	
	if(request->count == 0)
		goto forward;

	if(vector[0].iov_len < sizeof(struct RequestHeader))
		goto forward;

	header = (struct RequestHeader*) vector[0].iov_base;

	switch(header->major_opcode) {
		case XCB_MAP_WINDOW: {
			const struct xcb_map_window_request_t* req = concat_iovec_buffers<xcb_map_window_request_t>(vector, request->count);
			if(!req)
				break;

			fprintf(stderr, "[spotifywm] spotify MapWindow %x found\n", req->window);

			xcb_flush(c);
			
			// Don't use the same XCB connection as spotify is checking the returned number
			Display* dpy = XOpenDisplay(":0");
			XClassHint* classHint = XAllocClassHint();
			if (classHint) {
				classHint->res_name = (char*)"spotify";
				classHint->res_class = (char*)"spotify";
				int ret = XSetClassHint(dpy, req->window, classHint);
				fprintf(stderr, "XSetClassHint = %d\n", ret);
				XFree(classHint);
			}
			XSync(dpy, 1);
			XCloseDisplay(dpy);
			break;
		}

		case XCB_SEND_EVENT: {
			const struct xcb_send_event_request_t* req = concat_iovec_buffers<xcb_send_event_request_t>(vector, request->count);
			if(!req)
				break;
			struct xcb_client_message_event_t* event = (struct xcb_client_message_event_t*) req->event;

#ifdef DEBUG
			fprintf(stderr, "[spotifywm] spotify SendEvent %d (%s) {%d (%s), %d (%s), %d (%s), %d (%s), %d (%s)}\n",
				event->type, get_atom_name(event->type).c_str(),
				event->data.data32[0], get_atom_name(event->data.data32[0]).c_str(),
				event->data.data32[1], get_atom_name(event->data.data32[1]).c_str(),
				event->data.data32[2], get_atom_name(event->data.data32[2]).c_str(),
				event->data.data32[3], get_atom_name(event->data.data32[3]).c_str(),
				event->data.data32[4], get_atom_name(event->data.data32[4]).c_str());
#endif

			init_atoms();
			if(event->type == X11_ATOM_NET_WM_STATE && event->format == 32 && event->data.data32[0] == 1) {
				// Ignore requests to maximize window
				for(size_t i = 1; i < 3; i++) {
					if(event->data.data32[i] == X11_ATOM_NET_WM_STATE_MAXIMIZED_HORZ || event->data.data32[i] == X11_ATOM_NET_WM_STATE_MAXIMIZED_VERT) {
						event->data.data32[i] = XCB_ATOM_NONE;
					}
				}
			} else if(event->type == X11_ATOM_NET_ACTIVE_WINDOW) {
				// Ignore request to make focus on window, replace request with no operation
				header->major_opcode = XCB_NO_OPERATION;
			}
			break;
		}
	}

forward:
	return BASE(xcb_send_request)(c, flags, vector, request);
}
