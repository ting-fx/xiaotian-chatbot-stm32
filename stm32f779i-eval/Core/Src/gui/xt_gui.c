/* This is a small demo of the high-performance GUIX graphics framework. */

#include "main.h"
#include "xt_gui.h"
#include "gx_api.h"
#include "gx_display_driver_stm32f779i-eval_565rgb.h"
#include "xt_gui_resources.h"
#include "xt_gui_specifications.h"

#define  CANVAS_MEMORY_SIZE      (DISPLAY_1_X_RESOLUTION * DISPLAY_1_Y_RESOLUTION)

/* Define the ThreadX demo thread control block and stack.  */
TX_THREAD             gui_thread;
UCHAR                 gui_thread_stack[4096];
TX_THREAD             touch_thread;
ULONG                 touch_thread_stack[1024];
__attribute__((section(".SD_RAM"))) USHORT canvas_memory[CANVAS_MEMORY_SIZE];
GX_WINDOW_ROOT    *root;

/* Define prototypes.   */
VOID gui_thread_entry(ULONG thread_input);

void gui_setup()
{
    /* 创建Touch线程  */
    tx_thread_create(&touch_thread, "GUIX Touch Thread", touch_thread_entry, 0,
                     touch_thread_stack, sizeof(touch_thread_stack),
					 XT_CHAT_THREAD_PRIORITY - 2,
					 XT_CHAT_THREAD_PRIORITY - 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* 创建GUI线程  */
    tx_thread_create(&gui_thread, "GUI Thread", gui_thread_entry, 0,
                     gui_thread_stack, sizeof(gui_thread_stack),
					 XT_CHAT_THREAD_PRIORITY - 1,
					 XT_CHAT_THREAD_PRIORITY - 1, TX_NO_TIME_SLICE, TX_AUTO_START);
}

/******************************************************************************************/
/* Initiate and run GUIX.                                                                 */
/******************************************************************************************/
VOID gui_thread_entry(ULONG thread_input)
{
    /* Initialize GUIX. */
    gx_system_initialize();

    gx_studio_display_configure(DISPLAY_1, stm32f779_graphics_driver_setup_565rgb,
                                LANGUAGE_ENGLISH, DISPLAY_1_THEME_1, &root);

    gx_canvas_memory_define(root->gx_window_root_canvas, (GX_COLOR *)canvas_memory, CANVAS_MEMORY_SIZE * sizeof(USHORT));

    /* Create the main screen and attach it to root window. */
    gx_studio_named_widget_create("main_screen", (GX_WIDGET *)root, GX_NULL);

    /* Show the root window to make it and main screen visible.  */
    gx_widget_show(root);
    gx_widget_hide(&main_screen.main_screen_detected);
    gx_widget_hide(&main_screen.main_screen_recording);

    /* Let GUIX run */
    gx_system_start();

    ULONG actual_events;

    while(1)
    {
    	tx_event_flags_get(&xt_event_group, XT_EVENT_GUI_MASK, TX_OR_CLEAR , &actual_events, TX_WAIT_FOREVER);

    	if(actual_events & XT_EVENT_GUI_RECORDING)
    	{
			gx_widget_show(&main_screen.main_screen_recording);
			gx_widget_hide(&main_screen.main_screen_detected);
			gx_system_canvas_refresh();
    	}

    	if(actual_events & XT_EVENT_GUI_DETECTED)
    	{
    		gx_widget_show(&main_screen.main_screen_detected);
    		gx_widget_hide(&main_screen.main_screen_recording);
			gx_system_canvas_refresh();
    	}
    }
}
