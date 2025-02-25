#include "magma/logger/log.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <magma/backend/backend.h>
#include <magma/private/backend/backend.h>

#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
#include <xcb/dri3.h>

#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h>

#define UNUSED(x) ((void)x)

typedef struct magma_xcb_backend {
	magma_backend_t impl;

	xcb_connection_t *connection;
	xcb_window_t window;
	xcb_screen_t *screen;
	xcb_gcontext_t gc;
	xcb_visualid_t visual;
	xcb_colormap_t colormap;
	uint8_t depth;
} magma_xcb_backend_t;

/*VULKAN STUFF*/
void magma_xcb_backend_get_vk_exts(magma_backend_t *backend, char ***extensions,
		uint32_t *size) {
	static char *xcb_extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	};

	*extensions = xcb_extensions;
	*size = sizeof(xcb_extensions) / sizeof(xcb_extensions[0]);

	return;

	UNUSED(backend);
}

VkResult magma_xcb_backend_get_vk_surface(magma_backend_t *backend, VkInstance instance,
		VkSurfaceKHR *surface) {
	VkXcbSurfaceCreateInfoKHR create_info = { 0 };
	magma_xcb_backend_t *xcb = (void *)backend;

	create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	create_info.window = xcb->window;
	create_info.connection = xcb->connection;

	return vkCreateXcbSurfaceKHR(instance, &create_info, NULL, surface);
}

void magma_xcb_backend_deinit(magma_backend_t *backend) {
	magma_xcb_backend_t *xcb = (void *)backend;

	xcb_destroy_window(xcb->connection, xcb->window);

	xcb_disconnect(xcb->connection);

	free(xcb);
}

void magma_xcb_backend_start(magma_backend_t *backend) {
	magma_xcb_backend_t *xcb = (void *)backend;

	if(xcb->impl.keymap) {
		xcb->impl.keymap(backend, xcb->impl.keymap_data);
	}
	xcb_map_window(xcb->connection, xcb->window);

	xcb_flush(xcb->connection);
}



struct xkb_keymap *magma_xcb_backend_get_keymap(magma_backend_t *backend, struct xkb_context *context) {
	int device_id;
	magma_xcb_backend_t *xcb = (void*)backend;

	xkb_x11_setup_xkb_extension(xcb->connection, 1, 0, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
				NULL, NULL, NULL, NULL);	

	device_id = xkb_x11_get_core_keyboard_device_id(xcb->connection);
	if(device_id < 0) {
		printf("Device id error:\n");
		return NULL;	
	}

	return xkb_x11_keymap_new_from_device(context, xcb->connection,
			device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

struct xkb_state *magma_xcb_backend_get_xkbstate(magma_backend_t *backend, struct xkb_keymap *keymap) {
	int device_id;
	magma_xcb_backend_t *xcb = (void*)backend;
	
	device_id = xkb_x11_get_core_keyboard_device_id(xcb->connection);
	if(device_id < 0) {
		printf("Device id error:\n");
		return NULL;	
	}

	return xkb_x11_state_new_from_device(keymap, xcb->connection, device_id);
}


void magma_xcb_backend_put_buffer(magma_backend_t *backend, magma_buf_t *buffer) {
	magma_xcb_backend_t *xcb = (void *)backend;

	xcb_pixmap_t pix = xcb_generate_id(xcb->connection);

	xcb_dri3_pixmap_from_buffer(xcb->connection, pix, xcb->window, buffer->size, buffer->width,
			buffer->height, buffer->pitch, 32, 32, buffer->fd);

	xcb_copy_area(xcb->connection, pix, xcb->window, xcb->gc, 0, 0, 0, 0, buffer->width, buffer->height);

	xcb_free_pixmap(xcb->connection, pix);
}

/*TODO: IMPLEMENT CALLBACKS*/
void magma_xcb_backend_expose(magma_xcb_backend_t *xcb, xcb_expose_event_t *expose) {

	if(xcb->impl.draw) {
		xcb->impl.draw((void *)xcb, expose->height, expose->width, xcb->impl.draw_data);
	}
	

	xcb_flush(xcb->connection);
}

void magma_xcb_backend_configure(magma_xcb_backend_t *xcb, xcb_configure_notify_event_t *notify) {
	if(xcb->impl.resize) {
		xcb->impl.resize((void*)xcb, notify->height, notify->width, xcb->impl.resize_data);
	}
}

void magma_xcb_backend_key_press(magma_xcb_backend_t *xcb, xcb_key_press_event_t *press) {
	if(xcb->impl.key_press) {	
		xcb->impl.key_press((void*)xcb, press->detail, MAGMA_KEY_PRESS, xcb->impl.key_data);
	}
}

void magma_xcb_backend_key_release(magma_xcb_backend_t *xcb, xcb_key_release_event_t *release) {
	if(xcb->impl.key_press) {	
		xcb->impl.key_press((void*)xcb, release->detail, MAGMA_KEY_RELEASE, xcb->impl.key_data);
	}	

}


void magma_xcb_backend_button_press(magma_xcb_backend_t *xcb, xcb_button_press_event_t *press) {
	UNUSED(xcb);
	UNUSED(press);
}

void magma_xcb_backend_button_release(magma_xcb_backend_t *xcb, xcb_button_release_event_t *release) {
	UNUSED(xcb);
	UNUSED(release);
}

void magma_xcb_backend_pointer_motion(magma_xcb_backend_t *xcb, xcb_motion_notify_event_t *motion) {
	UNUSED(xcb);
	UNUSED(motion);
}

void magma_xcb_backend_enter(magma_xcb_backend_t *xcb, xcb_enter_notify_event_t *enter) {
	UNUSED(xcb);
	UNUSED(enter);
}

void magma_xcb_backend_leave(magma_xcb_backend_t *xcb, xcb_leave_notify_event_t *leave) {
	UNUSED(xcb);
	UNUSED(leave);
}

void magma_xcb_backend_dispacth(magma_backend_t *backend) {
	xcb_generic_event_t *event;
	magma_xcb_backend_t *xcb = (void *)backend;
	

	while((event = xcb_poll_for_event(xcb->connection))) {
		switch(event->response_type & ~0x80) {
			case XCB_EXPOSE:
				magma_xcb_backend_expose(xcb, (void *)event);
				break;
			case XCB_CONFIGURE_NOTIFY:
				magma_xcb_backend_configure(xcb, (void*)event);
				break;
			case XCB_KEY_PRESS:
				magma_xcb_backend_key_press(xcb, (void*)event);
				break;
			case XCB_KEY_RELEASE:
				magma_xcb_backend_key_release(xcb, (void*)event);
				break;
			case XCB_ENTER_NOTIFY:
				magma_xcb_backend_enter(xcb, (void*)event);
				break;
			case XCB_LEAVE_NOTIFY:
				magma_xcb_backend_leave(xcb, (void*)event);
				break;
			case XCB_MOTION_NOTIFY:
				magma_xcb_backend_pointer_motion(xcb, (void *)event);
				break;
			case XCB_BUTTON_PRESS:
				magma_xcb_backend_button_press(xcb, (void *)event);
				break;
			case XCB_BUTTON_RELEASE:
				magma_xcb_backend_button_release(xcb, (void *)event);
				break;
			case XCB_MAP_NOTIFY:
				break;
			default:
				printf("magma-xcb: unknown event %d\n", event->response_type);
		}
		xcb_flush(xcb->connection);
		free(event);
	}
}

xcb_visualtype_t *magma_xcb_match_visual(uint32_t depth, const xcb_screen_t *screen) {
	xcb_depth_iterator_t depths_iter;
	xcb_visualtype_iterator_t visual_iter;
	depths_iter = xcb_screen_allowed_depths_iterator(screen);
	
	for(; depths_iter.rem; xcb_depth_next(&depths_iter)) {
		
		if(depths_iter.data->depth == depth) {
			visual_iter = xcb_depth_visuals_iterator(depths_iter.data);
			for(; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
				magma_log_info("%d\n", visual_iter.data->visual_id);
				if(XCB_VISUAL_CLASS_TRUE_COLOR == visual_iter.data->_class) {
					return visual_iter.data;
				}
			}
		}
	}

	
	return NULL;
}

magma_backend_t *magma_xcb_backend_init(void) {
	magma_xcb_backend_t *xcb;
	xcb_screen_iterator_t iter;
	const xcb_setup_t *setup;
	xcb_void_cookie_t cookie;
	xcb_visualtype_t *visual;
	xcb_generic_error_t *error;
	int screen_nbr = 0;
	uint32_t mask, values[3];

	xcb = calloc(1, sizeof(*xcb));

	xcb->connection = xcb_connect(NULL, &screen_nbr);

	setup = xcb_get_setup(xcb->connection);

	iter = xcb_setup_roots_iterator(setup);

	for (; iter.rem; --screen_nbr, xcb_screen_next (&iter)) {
		if (screen_nbr == 0) {
			xcb->screen = iter.data;
			break;
		}
	}
	
	visual = magma_xcb_match_visual(32, xcb->screen);
	if(visual == NULL) {
		xcb->visual = xcb->screen->root_visual;
		xcb->depth = xcb->screen->root_depth;
		xcb->colormap = xcb->screen->default_colormap;
	} else {
		xcb->visual = visual->visual_id;
		xcb->depth = 32;
		xcb->colormap = xcb_generate_id(xcb->connection);

		cookie = xcb_create_colormap_checked(xcb->connection, XCB_COLORMAP_ALLOC_NONE, xcb->colormap, xcb->screen->root, xcb->visual);
		error = xcb_request_check(xcb->connection, cookie);
		if(error) {
			magma_log_error("XCB failed to create colormap\n");
			return NULL;
		}
	}

	xcb->window = xcb_generate_id(xcb->connection);
	mask = XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
	values[0] = 0x000000;
	values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
              XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
              XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
			  XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	values[2] = xcb->colormap;

	cookie = xcb_create_window_checked(xcb->connection, xcb->depth,
			xcb->window, xcb->screen->root, 0, 0, 600, 600, 1, 
			XCB_WINDOW_CLASS_INPUT_OUTPUT, xcb->visual, 
			mask, values);
	
	error = xcb_request_check(xcb->connection, cookie);
	if(error) {
		magma_log_error("Failed to create window: %d.%d.%d\n", error->error_code, error->major_code, error->minor_code);
		return NULL;
	}

	xcb->gc = xcb_generate_id(xcb->connection);
	mask = XCB_GC_FOREGROUND;
	values[0] = xcb->screen->black_pixel;
	cookie = xcb_create_gc_checked(xcb->connection, xcb->gc, xcb->window, mask, values);
	error = xcb_request_check(xcb->connection, cookie);
	if(error) {
		printf("Failed to create GC\n");
		return NULL;
	}

	magma_log_info("XCB Screen Info: %d\n", xcb->screen->root);
	magma_log_info("	width: %d\n", xcb->screen->width_in_pixels);
	magma_log_info("	height: %d\n", xcb->screen->height_in_pixels);

	/*KEYBOARD*/
	xcb->impl.get_kmap = magma_xcb_backend_get_keymap;
	xcb->impl.get_state = magma_xcb_backend_get_xkbstate;

	xcb->impl.start = magma_xcb_backend_start;
	xcb->impl.deinit = magma_xcb_backend_deinit;
	xcb->impl.dispatch_events = magma_xcb_backend_dispacth;
	

	xcb->impl.put_buffer = magma_xcb_backend_put_buffer;
	xcb->impl.magma_backend_get_vk_exts = magma_xcb_backend_get_vk_exts;
	xcb->impl.magma_backend_get_vk_surface = magma_xcb_backend_get_vk_surface;	

	return (void*)xcb;
}
