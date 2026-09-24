#import "TH095ViewController.hpp"
#import "TH095View.hpp"
@implementation TH095ViewController
- (BOOL)shouldAutorotate { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAll;
}
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeRight;
}
- (void)loadView { self.view = [[TH095View alloc] initWithFrame:CGRectZero]; }
@end
