#import <UIKit/UIKit.h>
#import "TH095AppDelegate.hpp"
#import "TH095ViewController.hpp"
#include "TH095Runtime.hpp"

@implementation TH095AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    TH095Runtime::shared().start(documents.UTF8String);
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[TH095ViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}
@end
