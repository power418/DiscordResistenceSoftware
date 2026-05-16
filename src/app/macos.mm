#import <AppKit/AppKit.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include <rpc/platform/macos_bridge.h>
#include <rpc/platform/recent_activity_dialog.hpp>
#include <rpc/platform/settings_dialog.hpp>
#include <rpc/platform/tray.hpp>

#include <modules/rpc/config.cppm>
#include <modules/rpc/core.cppm>
#include <modules/rpc/os/autostart.cppm>

namespace {

[[nodiscard]] NSString* ns_from_wide(std::wstring_view value) {
  if (value.empty()) {
    return @"";
  }

  NSString* converted = [[NSString alloc]
    initWithBytes:value.data()
           length:value.size() * sizeof(wchar_t)
         encoding:NSUTF32LittleEndianStringEncoding];
  return converted != nil ? converted : @"";
}

[[nodiscard]] NSString* ns_from_utf8(std::string_view value) {
  if (value.empty()) {
    return @"";
  }

  NSString* converted = [[NSString alloc]
    initWithBytes:value.data()
           length:value.size()
         encoding:NSUTF8StringEncoding];
  return converted != nil ? converted : @"";
}

[[nodiscard]] NSString* app_title() {
  const std::string title = rpc::app_name();
  return ns_from_utf8(title.empty() ? std::string_view("software_discord_rpc") : std::string_view(title));
}

[[nodiscard]] NSImage* load_status_image() {
  static NSArray<NSString*>* const kCandidates = @[
    @"res/discord_rpc_transparent.png",
    @"res/discord_rpc.png",
    @"../res/discord_rpc_transparent.png",
    @"../res/discord_rpc.png",
  ];

  NSFileManager* file_manager = [NSFileManager defaultManager];
  for (NSString* path in kCandidates) {
    if ([file_manager fileExistsAtPath:path]) {
      NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];
      if (image != nil) {
        [image setSize:NSMakeSize(18.0, 18.0)];
        [image setTemplate:YES];
        return image;
      }
    }
  }

  NSImage* fallback = [NSImage imageNamed:NSImageNameApplicationIcon];
  if (fallback != nil) {
    [fallback setSize:NSMakeSize(18.0, 18.0)];
  }
  return fallback;
}

[[nodiscard]] NSButton* make_checkbox(NSString* title, BOOL state, NSRect frame) {
  NSButton* button = [[NSButton alloc] initWithFrame:frame];
  button.buttonType = NSButtonTypeSwitch;
  button.title = title;
  button.state = state ? NSControlStateValueOn : NSControlStateValueOff;
  button.bezelStyle = NSBezelStyleRegularSquare;
  return button;
}

[[nodiscard]] NSTextField* make_label(NSString* title, NSRect frame, CGFloat size = 11.0) {
  NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
  label.stringValue = title;
  label.editable = NO;
  label.selectable = NO;
  label.bezeled = NO;
  label.drawsBackground = NO;
  label.font = [NSFont systemFontOfSize:size];
  label.textColor = [NSColor secondaryLabelColor];
  return label;
}

} // namespace

@interface RPCTrayController : NSObject
@property(nonatomic, strong) NSStatusItem* statusItem;
@property(nonatomic, strong) NSMenu* menu;
@end

@implementation RPCTrayController

- (void)showMain:(id)sender {
  (void)sender;
  rpc_macos_show_main_window();
}

- (void)showRecent:(id)sender {
  (void)sender;
  rpc_macos_show_recent_activity();
}

- (void)showSettings:(id)sender {
  (void)sender;
  rpc_macos_show_settings();
}

- (void)quit:(id)sender {
  (void)sender;
  rpc_macos_request_quit();
}

@end

static RPCTrayController* g_tray_controller = nil;
static std::uint32_t g_last_tray_error = 0;

namespace rpc::platform {

bool init_tray_platform() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];
    g_last_tray_error = 0;
    return true;
  }
}

bool add_tray_icon(const TrayConfig& config) {
  @autoreleasepool {
    if (g_tray_controller != nil && g_tray_controller.statusItem != nil) {
      [[NSStatusBar systemStatusBar] removeStatusItem:g_tray_controller.statusItem];
    }

    RPCTrayController* controller = [[RPCTrayController alloc] init];
    NSStatusItem* status_item =
      [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    if (status_item == nil) {
      g_last_tray_error = 1;
      return false;
    }

    NSImage* icon = load_status_image();
    NSStatusBarButton* button = status_item.button;
    if (button == nil) {
      [[NSStatusBar systemStatusBar] removeStatusItem:status_item];
      g_last_tray_error = 2;
      return false;
    }

    if (icon != nil) {
      button.image = icon;
      button.imagePosition = NSImageOnly;
      button.title = @"";
    } else {
      status_item.length = NSVariableStatusItemLength;
      button.title = @"RPC";
    }
    button.toolTip = ns_from_wide(
      config.tooltip != nullptr ? std::wstring_view(config.tooltip) : std::wstring_view());

    NSString* title = app_title();
    NSMenu* menu = [[NSMenu alloc] initWithTitle:title];

    NSMenuItem* header =
      [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    header.enabled = NO;
    [menu addItem:header];
    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* show_item =
      [[NSMenuItem alloc] initWithTitle:@"Show" action:@selector(showMain:) keyEquivalent:@""];
    show_item.target = controller;
    [menu addItem:show_item];

    NSMenuItem* recent_item = [[NSMenuItem alloc]
      initWithTitle:@"Recent activity"
      action:@selector(showRecent:)
      keyEquivalent:@""];
    recent_item.target = controller;
    [menu addItem:recent_item];

    NSMenuItem* settings_item =
      [[NSMenuItem alloc] initWithTitle:@"Settings" action:@selector(showSettings:) keyEquivalent:@""];
    settings_item.target = controller;
    [menu addItem:settings_item];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* exit_item =
      [[NSMenuItem alloc] initWithTitle:@"Exit" action:@selector(quit:) keyEquivalent:@""];
    exit_item.target = controller;
    [menu addItem:exit_item];

    controller.statusItem = status_item;
    controller.menu = menu;
    status_item.menu = menu;
    g_tray_controller = controller;
    g_last_tray_error = 0;
    return true;
  }
}

bool show_tray_balloon(void*,
                       std::uint32_t,
                       const wchar_t* title,
                       const wchar_t* message) {
  @autoreleasepool {
    NSUserNotificationCenter* center = [NSUserNotificationCenter defaultUserNotificationCenter];
    if (center == nil) {
      g_last_tray_error = 0;
      return true;
    }

    NSUserNotification* notification = [[NSUserNotification alloc] init];
    notification.title = title != nullptr ? ns_from_wide(std::wstring_view(title)) : app_title();
    notification.informativeText = ns_from_wide(message != nullptr ? std::wstring_view(message) : std::wstring_view());
    notification.soundName = nil;

    [center deliverNotification:notification];
    g_last_tray_error = 0;
    return true;
  }
}

void remove_tray_icon(void*, std::uint32_t) {
  @autoreleasepool {
    if (g_tray_controller != nil && g_tray_controller.statusItem != nil) {
      [[NSStatusBar systemStatusBar] removeStatusItem:g_tray_controller.statusItem];
      g_tray_controller.statusItem = nil;
      g_tray_controller.menu = nil;
    }

    g_tray_controller = nil;
    g_last_tray_error = 0;
  }
}

void show_tray_context_menu(void*) {
  @autoreleasepool {
    if (g_tray_controller == nil || g_tray_controller.statusItem == nil || g_tray_controller.menu == nil) {
      g_last_tray_error = 1;
      return;
    }

    [NSApp activateIgnoringOtherApps:YES];
    [g_tray_controller.statusItem popUpStatusItemMenu:g_tray_controller.menu];
    g_last_tray_error = 0;
  }
}

std::uint32_t last_tray_error() {
  return g_last_tray_error;
}

void show_settings_dialog(void*, void*, void*, rpc::Config& config) {
  @autoreleasepool {
    [NSApp activateIgnoringOtherApps:YES];

    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleInformational;
    alert.messageText = @"Settings";
    alert.informativeText = @"Configure tray behavior and history preferences.";
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];

    NSView* accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 390, 138)];
    NSButton* autostart = make_checkbox(@"Run at startup", config.autostart, NSMakeRect(0, 96, 260, 18));
    NSButton* generic_mode = make_checkbox(@"Generic Mode (Productive Apps)", config.generic_mode, NSMakeRect(0, 68, 330, 18));
    NSButton* show_file_name = make_checkbox(@"Show File/Project Name", config.show_file_name, NSMakeRect(0, 40, 260, 18));
    NSTextField* note = make_label(
      @"Autostart preference is saved locally on macOS.",
      NSMakeRect(0, 4, 390, 24));

    [accessory addSubview:autostart];
    [accessory addSubview:generic_mode];
    [accessory addSubview:show_file_name];
    [accessory addSubview:note];
    alert.accessoryView = accessory;

    const NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
      config.autostart = (autostart.state == NSControlStateValueOn);
      config.generic_mode = (generic_mode.state == NSControlStateValueOn);
      config.show_file_name = (show_file_name.state == NSControlStateValueOn);
      rpc::os::set_autostart_enabled(config.autostart);
      rpc::save_config(rpc::settings_path(), config);
    }
  }
}

void show_recent_activity_dialog(void*,
                                 void*,
                                 void*,
                                 std::string_view title,
                                 std::string_view summary) {
  @autoreleasepool {
    [NSApp activateIgnoringOtherApps:YES];

    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleInformational;
    alert.messageText = ns_from_utf8(title.empty() ? std::string_view("Recent activity") : title);
    alert.informativeText = ns_from_utf8(summary.empty()
      ? std::string_view("No recent activity yet. Open a supported app to build history.")
      : summary);
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
  }
}

} // namespace rpc::platform

extern "C" void rpc_macos_run_application(void) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp run];
  }
}

extern "C" void rpc_macos_activate_application(void) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp activateIgnoringOtherApps:YES];
  }
}

extern "C" void rpc_macos_request_quit(void) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp terminate:nil];
  }
}

extern "C" void rpc_macos_show_splash_dialog(const char* title, const char* message) {
  @autoreleasepool {
    [NSApp activateIgnoringOtherApps:YES];

    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleInformational;
    alert.messageText = ns_from_utf8(title != nullptr ? std::string_view(title) : std::string_view("software_discord_rpc"));
    alert.informativeText = ns_from_utf8(message != nullptr ? std::string_view(message) : std::string_view("Discord RPC monitor active."));
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
  }
}
