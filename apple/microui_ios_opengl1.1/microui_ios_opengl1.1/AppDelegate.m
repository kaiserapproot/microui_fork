#import "AppDelegate.h"
#import <UIKit/UIKit.h>
// MicroUIViewControllerの宣言（ios_opengl1.1_microui.mで実装済みと仮定）
@interface MicroUIViewController : UIViewController @end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[MicroUIViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

@end
