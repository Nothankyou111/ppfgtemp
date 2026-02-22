// scenes/protopirate_scene_receiver_info.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_RECEIVER_SCENE

#include "../helpers/protopirate_storage.h"

#define TAG "ProtoPirateReceiverInfo"

static bool is_emu_off = false;

static void protopirate_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;
    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeRight) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSave);
        }
#ifdef ENABLE_EMULATE_FEATURE
        else if(result == GuiButtonTypeLeft && !is_emu_off) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventReceiverInfoEmulate);
        }
#endif
    }
}

void protopirate_scene_receiver_info_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    //Stop chargin while using the radio.
    furi_hal_power_suppress_charge_enter();

    // Always reset per-enter to avoid stale static state
    is_emu_off = false;

    widget_reset(app->widget);

    FuriString* text;
    text = furi_string_alloc();

    protopirate_history_get_text_item_menu(app->txrx->history, text, app->txrx->idx_menu_chosen);

    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));

    furi_string_reset(text);
    protopirate_history_get_text_item(app->txrx->history, text, app->txrx->idx_menu_chosen);

    bool is_psa = false;
    FlipperFormat* ff =
        protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(ff) {
        FuriString* protocol = furi_string_alloc();
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Protocol", protocol)) {
            if(furi_string_cmp_str(protocol, "PSA") == 0) {
                is_psa = true;
            }
            if(furi_string_cmp_str(protocol, "Scher-Khan") == 0) {
                is_emu_off = true;
            } else if(furi_string_cmp_str(protocol, "Kia V6") == 0) {
                is_emu_off = true;
            } else {
                is_emu_off = false;
            }
        }
        furi_string_free(protocol);
    }

    // Skip the first line (protocol name + Xbits) since it's already shown as the title
    const char* text_str = furi_string_get_cstr(text);
    const char* first_newline = strchr(text_str, '\r');
    if(first_newline) {
        // Skip \r\n
        text_str = first_newline + 1;
        if(*text_str == '\n') {
            text_str++;
        }
    } else {
        // Try \n only
        first_newline = strchr(text_str, '\n');
        if(first_newline) {
            text_str = first_newline + 1;
        }
    }

    if(is_psa) {
        FuriString* reformatted = furi_string_alloc();
        const char* current = text_str;

        while(*current) {
            const char* line_end = strchr(current, '\r');
            if(!line_end) {
                line_end = strchr(current, '\n');
            }
            if(!line_end) {
                line_end = current + strlen(current);
            }

            if(strncmp(current, "Ser:", 4) == 0) {
                size_t ser_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s", (int)ser_len, current);

                const char* next_line = line_end;
                if(*next_line == '\r') next_line++;
                if(*next_line == '\n') next_line++;

                if(strncmp(next_line, "Cnt:", 4) == 0) {
                    const char* cnt_end = strchr(next_line, '\r');
                    if(!cnt_end) {
                        cnt_end = strchr(next_line, '\n');
                    }
                    if(!cnt_end) {
                        cnt_end = next_line + strlen(next_line);
                    }

                    size_t cnt_len = cnt_end - next_line;
                    furi_string_cat_printf(reformatted, " %.*s\r\n", (int)cnt_len, next_line);

                    current = cnt_end;
                    if(*current == '\r') current++;
                    if(*current == '\n') current++;
                } else {
                    furi_string_cat_printf(reformatted, "\r\n");
                    current = line_end;
                    if(*current == '\r') current++;
                    if(*current == '\n') current++;
                }
            } else {
                size_t line_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s\r\n", (int)line_len, current);
                current = line_end;
                if(*current == '\r') current++;
                if(*current == '\n') current++;
            }

            if(*current == '\0') break;
        }

        widget_add_string_multiline_element(
            app->widget,
            0,
            11,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(reformatted));
        furi_string_free(reformatted);
    } else {
        widget_add_string_multiline_element(
            app->widget, 0, 11, AlignLeft, AlignTop, FontSecondary, text_str);
    }

#ifdef ENABLE_EMULATE_FEATURE
    // Add emulate button on the left
    if(!is_emu_off) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Emulate",
            protopirate_scene_receiver_info_widget_callback,
            app);
    }
#endif

    // Add save button on the right
    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        "Save",
        protopirate_scene_receiver_info_widget_callback,
        app);

    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventReceiverInfoSave) {
            // Get the flipper format from history
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);

            FuriString* filename_str = furi_string_alloc();

            if(ff) {
                if(app->option_flags & FLAG_DATETIME_FILENAMES) {
                    //Get the date and time to save.
                    DateTime date_time;
                    furi_hal_rtc_get_datetime(&date_time);

                    furi_string_printf(
                        filename_str,
                        "%.2d%.2d%.2d_%.2d.%.2d.%.2d_",
                        date_time.year,
                        date_time.month,
                        date_time.day,
                        date_time.hour,
                        date_time.minute,
                        date_time.second);
                }

                // Extract protocol name
                FuriString* protocol = furi_string_alloc();
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, "Protocol", protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                //Add the protocol
                furi_string_cat(filename_str, protocol);
                furi_string_free(protocol);

                // Clean protocol name for filename
                furi_string_replace_all(filename_str, "/", "_");
                furi_string_replace_all(filename_str, " ", "_");

                FuriString* saved_path = furi_string_alloc();
                if(protopirate_storage_save_capture(
                       ff,
                       furi_string_get_cstr(filename_str),
                       saved_path,
                       (app->option_flags & FLAG_DATETIME_FILENAMES))) {
                    // Show success notification
                    notification_message(app->notifications, &sequence_success);
                    FURI_LOG_I(TAG, "Saved to: %s", furi_string_get_cstr(saved_path));
                } else {
                    notification_message(app->notifications, &sequence_error);
                    FURI_LOG_E(TAG, "Save failed");
                }

                furi_string_free(filename_str);
                furi_string_free(saved_path);
            }
            consumed = true;
        }
#ifdef ENABLE_EMULATE_FEATURE
        else if(event.event == ProtoPirateCustomEventReceiverInfoEmulate && !is_emu_off) {
            // Get the flipper format from history
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);

            if(ff) {
                // Save to temp file (will be deleted on emulate exit)
                if(protopirate_storage_save_temp(ff)) {
                    FURI_LOG_I(TAG, "Saved temp for emulate");

                    // Set the temp file path for emulate scene
                    if(app->loaded_file_path) {
                        furi_string_free(app->loaded_file_path);
                    }
                    app->loaded_file_path = furi_string_alloc_set_str(PROTOPIRATE_TEMP_FILE);

                    //Allocate the About View.
                    app->view_about = view_alloc();
                    view_dispatcher_add_view(
                        app->view_dispatcher, ProtoPirateViewAbout, app->view_about);

                    // Go to emulate scene
                    scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
                } else {
                    notification_message(app->notifications, &sequence_error);
                    FURI_LOG_E(TAG, "Failed to save temp for emulate");
                }
            } else {
                FURI_LOG_E(TAG, "No flipper format data for index %d", app->txrx->idx_menu_chosen);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
#endif
    }

    return consumed;
}

void protopirate_scene_receiver_info_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    widget_reset(app->widget);
    furi_hal_power_suppress_charge_exit();
}
#endif //ENABLE_RECEIVER_SCENE
