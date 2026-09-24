#import <UIKit/UIKit.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "touch.h"

typedef void (*EIKFrameCallback)(void);
typedef void (*EIKAudioLifecycleCallback)(void);

static EIKAudioLifecycleCallback audio_pause_callback;
static EIKAudioLifecycleCallback audio_resume_callback;

static void audio_session_will_resign_active(NSNotification *notification)
{
    (void)notification;
    if (audio_pause_callback != NULL) {
        audio_pause_callback();
    }
}

static void audio_session_did_become_active(NSNotification *notification)
{
    (void)notification;
    if (audio_resume_callback != NULL) {
        audio_resume_callback();
    }
}

@interface EIKIOSFrameDriver : NSObject {
    void *_sdl_window;
    EIKFrameCallback _frame_callback;
    CADisplayLink *_display_link;
    BOOL _is_attached;
}

- (instancetype)initWithSDLWindow:(void *)sdl_window frameCallback:(EIKFrameCallback)frame_callback;
- (void)start;
- (void)runFrame:(CADisplayLink *)display_link;
@end

static EIKIOSFrameDriver *frame_driver;

static BOOL attach_window_to_active_scene(void *sdl_window)
{
    SDL_SysWMinfo window_info;
    UIWindowScene *active_scene = nil;
    UIWindow *window = nil;

    if (sdl_window == NULL) {
        return NO;
    }

    SDL_VERSION(&window_info.version);
    if (SDL_GetWindowWMInfo((SDL_Window *)sdl_window, &window_info) == SDL_FALSE
        || window_info.subsystem != SDL_SYSWM_UIKIT) {
        return NO;
    }

    window = window_info.info.uikit.window;
    if (window == nil) {
        return NO;
    }

    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (scene.activationState == UISceneActivationStateForegroundActive
            && [scene isKindOfClass:[UIWindowScene class]]) {
            active_scene = (UIWindowScene *)scene;
            break;
        }
    }

    if (active_scene == nil) {
        return NO;
    }

    window.windowScene = active_scene;
    [window makeKeyAndVisible];
    return YES;
}

@implementation EIKIOSFrameDriver

- (instancetype)initWithSDLWindow:(void *)sdl_window frameCallback:(EIKFrameCallback)frame_callback
{
    self = [super init];
    if (self != nil) {
        _sdl_window = sdl_window;
        _frame_callback = frame_callback;
    }
    return self;
}

- (void)runFrame:(CADisplayLink *)display_link
{
    (void)display_link;
    if (!_is_attached) {
        _is_attached = attach_window_to_active_scene(_sdl_window);
    }
    if (_is_attached && _frame_callback != NULL) {
        _frame_callback();
    }
}

- (void)start
{
    _display_link = [CADisplayLink displayLinkWithTarget:self selector:@selector(runFrame:)];
    _display_link.preferredFramesPerSecond = 60;
    [_display_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

@end

void eik_ios_start_frame_loop(void *sdl_window, EIKFrameCallback frame_callback)
{
    frame_driver = [[EIKIOSFrameDriver alloc] initWithSDLWindow:sdl_window
        frameCallback:frame_callback];
    [frame_driver start];
}

void eik_ios_configure_audio_session(void)
{
    NSError *error = nil;
    AVAudioSession *session = AVAudioSession.sharedInstance;

    if (![session setCategory:AVAudioSessionCategoryAmbient
            withOptions:AVAudioSessionCategoryOptionMixWithOthers error:&error]) {
        NSLog(@"Edgard in Kimeria: could not configure ambient audio session: %@", error);
        return;
    }
    if (![session setActive:YES error:&error]) {
        NSLog(@"Edgard in Kimeria: could not activate ambient audio session: %@", error);
    }
}

void eik_ios_set_audio_lifecycle_callbacks(EIKAudioLifecycleCallback pause_callback,
    EIKAudioLifecycleCallback resume_callback)
{
    NSNotificationCenter *notifications = NSNotificationCenter.defaultCenter;

    audio_pause_callback = pause_callback;
    audio_resume_callback = resume_callback;
    [notifications addObserverForName:UIApplicationWillResignActiveNotification object:nil
        queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *notification) {
            audio_session_will_resign_active(notification);
        }];
    [notifications addObserverForName:UIApplicationDidBecomeActiveNotification object:nil
        queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *notification) {
            audio_session_did_become_active(notification);
        }];
}

EikTouchSafeArea eik_ios_safe_area(void *sdl_window)
{
    SDL_SysWMinfo window_info;
    EikTouchSafeArea safe_area = { 0.0F, 0.0F, 0.0F, 0.0F };

    if (sdl_window == NULL) {
        return safe_area;
    }
    SDL_VERSION(&window_info.version);
    if (SDL_GetWindowWMInfo((SDL_Window *)sdl_window, &window_info) == SDL_FALSE
            || window_info.subsystem != SDL_SYSWM_UIKIT
            || window_info.info.uikit.window == nil) {
        return safe_area;
    }
    {
        const UIEdgeInsets insets = window_info.info.uikit.window.safeAreaInsets;

        safe_area.left = (float)insets.left;
        safe_area.top = (float)insets.top;
        safe_area.right = (float)insets.right;
        safe_area.bottom = (float)insets.bottom;
    }
    return safe_area;
}
