#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef enum {
    MenuStateSingleMode,
    MenuStateCoopMode,
    MenuStateGameStarting
} MenuState;

typedef struct {
    MenuState state;
} Interface;

typedef enum {
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} InterfaceEvent;

static void render_callback(Canvas* const canvas, void* ctx) {
    const Interface* interface = acquire_mutex((ValueMutex*)ctx, 25);
    if(interface == NULL) {
        return;
    }

    // Before the function is called, the state is set with the canvas_reset(canvas)

    // Frame
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    if (interface->state == MenuStateGameStarting) {
        canvas_draw_icon(
            canvas,
            0,
            0,
            &I_TanksSplashScreen_128x64
        );
    } else {
        canvas_draw_icon(
            canvas,
            0,
            0,
            &I_TanksSplashScreen_128x64
        );
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 110, 10, AlignRight, AlignBottom, "Single");
        canvas_draw_str_aligned(canvas, 110, 30, AlignRight, AlignBottom, "Co-op");
    }

    canvas_draw_frame(canvas, 0, 0, 128, 64);

     if(interface->state == MenuStateSingleMode) {
        canvas_draw_icon(canvas, 70,3, &I_TankRight_6x6);
    } else if (interface->state == MenuStateCoopMode) {
        canvas_draw_icon(canvas,70,23, &I_TankRight_6x6);
    }

    release_mutex((ValueMutex*)ctx, interface);
}

static void input_callback(InputEvent* input_event, osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    InterfaceEvent event = {.type = EventTypeKey, .input = *input_event};
    osMessageQueuePut(event_queue, &event, 0, osWaitForever);
}

int32_t tanks_menu_app(void* p) {
    srand(DWT->CYCCNT);

    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(InterfaceEvent), NULL);

    Interface* interface = furi_alloc(sizeof(Interface));
    interface->state = MenuStateSingleMode;

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, interface, sizeof(Interface))) {
        furi_log_print(FURI_LOG_ERROR, "cannot create mutex\r\n");
        free(interface);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InterfaceEvent event;
    for(bool processing = true; processing;) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);

        Interface* interface = (Interface*)acquire_mutex_block(&state_mutex);

        if(event_status == osOK) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                    case InputKeyDown:
                        if(interface->state == MenuStateSingleMode) {
                            interface->state = MenuStateCoopMode;
                        } else if (interface->state == MenuStateCoopMode) {
                            interface->state = MenuStateSingleMode;
                        }
                    case InputKeyRight:
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        if(interface->state == MenuStateSingleMode || interface->state == MenuStateCoopMode) {
                            interface->state = MenuStateGameStarting;
                        }
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            }
        } else {
            // event timeout
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, interface);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);
    delete_mutex(&state_mutex);
    free(interface);

    return 0;
}