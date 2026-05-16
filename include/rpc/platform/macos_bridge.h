#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void rpc_macos_run_application(void);
void rpc_macos_activate_application(void);
void rpc_macos_request_quit(void);

void rpc_macos_show_main_window(void);
void rpc_macos_show_recent_activity(void);
void rpc_macos_show_settings(void);
void rpc_macos_show_splash_dialog(const char* title, const char* message);


#ifdef __cplusplus
} // extern "C"
#endif
