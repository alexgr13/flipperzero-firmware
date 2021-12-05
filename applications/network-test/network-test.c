#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include <lib/toolbox/args.h>
#include <lib/subghz/subghz_parser.h>
#include <lib/subghz/subghz_keystore.h>
#include <lib/subghz/protocols/subghz_protocol_common.h>
#include <lib/subghz/protocols/subghz_protocol_princeton.h>
#include <lib/subghz/subghz_tx_rx_worker.h>

#include <notification/notification-messages.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} NetworkTestEvent;

typedef struct {
    bool isServerSet;
    bool isServer;
    bool isPixel;
    bool isServerStringSet;
    bool isClientStringSet;
    string_t serverString;
    string_t clientString;
} NetworkTestState;

static void network_test_render_callback(Canvas* const canvas, void* ctx) {
    const NetworkTestState* network_test_state = acquire_mutex((ValueMutex*)ctx, 25);

    if(network_test_state == NULL) {
        return;
    }

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    if(network_test_state->isServer && network_test_state->isServerStringSet) {
        canvas_draw_str(canvas, 37, 31, "Server");
        canvas_draw_str_aligned(
            canvas,
            64,
            41,
            AlignCenter,
            AlignBottom,
            string_get_cstr(network_test_state->serverString));
    }

    if(!network_test_state->isServer && network_test_state->isClientStringSet) {
        canvas_draw_str(canvas, 37, 31, "Client");
        canvas_draw_str_aligned(
            canvas,
            64,
            41,
            AlignCenter,
            AlignBottom,
            string_get_cstr(network_test_state->clientString));
    }

    release_mutex((ValueMutex*)ctx, network_test_state);
}

static void network_test_input_callback(InputEvent* input_event, osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    NetworkTestEvent event = {.type = EventTypeKey, .input = *input_event};
    osMessageQueuePut(event_queue, &event, 0, osWaitForever);
}

static void network_test_update_timer_callback(osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    NetworkTestEvent event = {.type = EventTypeTick};
    osMessageQueuePut(event_queue, &event, 0, 0);
}

int32_t network_test_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(NetworkTestEvent), NULL);
    NetworkTestState* network_test_state = furi_alloc(sizeof(NetworkTestState));
    network_test_state->isServerSet = false;
    network_test_state->isServer = false;
    network_test_state->isServerStringSet = false;
    network_test_state->isClientStringSet = false;
    // network_test_state->isPixel = false;
    ValueMutex state_mutex;

    if(!init_mutex(&state_mutex, network_test_state, sizeof(NetworkTestState))) {
        furi_log_print(FURI_LOG_ERROR, "cannot create mutex\r\n");
        free(network_test_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, network_test_render_callback, &state_mutex);
    view_port_input_callback_set(view_port, network_test_input_callback, event_queue);

    osTimerId_t timer =
        osTimerNew(network_test_update_timer_callback, osTimerPeriodic, event_queue, NULL);
    osTimerStart(timer, osKernelGetTickFreq() / 4);
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NetworkTestEvent event;

    uint32_t frequency = 433920000;
    size_t message_max_len = 64;
    uint8_t incomingMessage[64] = {0};
    SubGhzTxRxWorker* subghz_txrx = subghz_tx_rx_worker_alloc();
    subghz_tx_rx_worker_start(subghz_txrx, frequency);
    furi_hal_power_suppress_charge_enter();
    // string_t outgoingMessage;
    // string_init(outgoingMessage);
    // string_t yesMessage;
    // string_init(yesMessage);
    // string_set(yesMessage, "y");

    for(bool processing = true; processing;) {
        srand(DWT->CYCCNT);
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);
        NetworkTestState* network_test_state =
            (NetworkTestState*)acquire_mutex_block(&state_mutex);

        if(event_status == osOK) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(network_test_state->isServerSet) {
                            break;
                        } else {
                            network_test_state->isServerSet = true;
                            network_test_state->isServer = true;

                            char arr[11];

                            for(int i = 0; i < 10; i++) {
                                arr[i] = rand() % 26 + 40;
                            }

                            arr[10] = 0;

                            string_set(network_test_state->serverString, (char*)&arr);
                            network_test_state->isServerStringSet = true;
                        }

                        break;
                    case InputKeyDown:
                        if(network_test_state->isServerSet) {
                            break;
                        } else {
                            network_test_state->isServerSet = true;
                            network_test_state->isServer = false;
                        }
                        break;
                    case InputKeyRight:
                        break;
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        if(!network_test_state->isServerSet || !network_test_state->isServer) {
                            break;
                        }

                        subghz_tx_rx_worker_write(
                            subghz_txrx,
                            (uint8_t*)string_get_cstr(network_test_state->serverString),
                            strlen(string_get_cstr(network_test_state->serverString)));

                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                //  snake_game_process_game_step(network_test_state);
                if(subghz_tx_rx_worker_available(subghz_txrx)) {
                    memset(incomingMessage, 0x00, message_max_len);
                    subghz_tx_rx_worker_read(subghz_txrx, incomingMessage, message_max_len);
                    string_set(network_test_state->clientString, (char*)incomingMessage);
                    network_test_state->isClientStringSet = true;
                }
            }
        } else {
            // event timeout
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, network_test_state);
        osDelay(1);
    }

    osDelay(10);
    furi_hal_power_suppress_charge_exit();

    if(subghz_tx_rx_worker_is_running(subghz_txrx)) {
        subghz_tx_rx_worker_stop(subghz_txrx);
        subghz_tx_rx_worker_free(subghz_txrx);
    }

    osTimerDelete(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);
    delete_mutex(&state_mutex);
    free(network_test_state);

    return 0;
}