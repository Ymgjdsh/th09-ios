#include "ios_cheat.hpp"
#include "ios_touch.hpp"

#include <UIKit/UIKit.h>
#include <dispatch/dispatch.h>

namespace th095
{
namespace modern
{
namespace ios
{

static UIViewController *TopViewController()
{
    UIApplication *application = [UIApplication sharedApplication];
    UIWindow *window = application.keyWindow;
    if (window == nil)
    {
        for (UIWindow *candidate in application.windows)
        {
            if (candidate.hidden == NO && candidate.rootViewController != nil)
            {
                window = candidate;
                if (candidate.isKeyWindow)
                    break;
            }
        }
    }

    UIViewController *controller = window.rootViewController;
    while (controller.presentedViewController != nil)
        controller = controller.presentedViewController;
    return controller;
}

void ShowCheatCodeDialog()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController *presenter = TopViewController();
        if (presenter == nil)
        {
            LogStartup("cheat-dialog: no presenter available");
            return;
        }

        UIAlertController *dialog =
            [UIAlertController alertControllerWithTitle:@"输入作弊码"
                                                message:@"请输入作弊码以解锁全部内容"
                                         preferredStyle:UIAlertControllerStyleAlert];
        [dialog addTextFieldWithConfigurationHandler:^(UITextField *field) {
            field.placeholder = @"作弊码";
            field.autocapitalizationType = UITextAutocapitalizationTypeNone;
            field.autocorrectionType = UITextAutocorrectionTypeNo;
            field.secureTextEntry = NO;
            field.keyboardType = UIKeyboardTypeASCIICapable;
        }];

        UIAlertAction *cancel =
            [UIAlertAction actionWithTitle:@"取消"
                                     style:UIAlertActionStyleCancel
                                   handler:nil];
        UIAlertAction *confirm =
            [UIAlertAction actionWithTitle:@"确定"
                                     style:UIAlertActionStyleDefault
                                   handler:^(UIAlertAction *) {
            NSString *value = dialog.textFields.firstObject.text;
            const bool accepted = ApplyIosCheatCode(value.UTF8String);
            if (!accepted)
            {
                UIAlertController *invalid =
                    [UIAlertController alertControllerWithTitle:@"作弊码无效"
                                                        message:@"没有应用任何存档修改"
                                                 preferredStyle:UIAlertControllerStyleAlert];
                [invalid addAction:[UIAlertAction actionWithTitle:@"好"
                                                              style:UIAlertActionStyleDefault
                                                            handler:nil]];
                [presenter presentViewController:invalid animated:YES completion:nil];
            }
            else
            {
                UIAlertController *success =
                    [UIAlertController alertControllerWithTitle:@"解锁完成"
                                                        message:@"全部内容已写入存档，返回标题页后即可使用"
                                                 preferredStyle:UIAlertControllerStyleAlert];
                [success addAction:[UIAlertAction actionWithTitle:@"好"
                                                              style:UIAlertActionStyleDefault
                                                            handler:nil]];
                [presenter presentViewController:success animated:YES completion:nil];
            }
        }];
        [dialog addAction:cancel];
        [dialog addAction:confirm];
        [presenter presentViewController:dialog animated:YES completion:nil];
    });
}

} // namespace ios
} // namespace modern
} // namespace th095
