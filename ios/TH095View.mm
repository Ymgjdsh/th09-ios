#import "TH095View.hpp"
#include "TH095Runtime.hpp"

@interface TH095View () {
    CADisplayLink *_displayLink;
    CGPoint _touchPoint;
    BOOL _touchActive;
    BOOL _zHeld;
    BOOL _sHeld;
    CGImageRef _titleImage;
    CGImageRef _worldImage;
}
@end

@implementation TH095View

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.multipleTouchEnabled = YES;
        self.backgroundColor = [UIColor blackColor];
        _titleImage = nil;
        _worldImage = nil;
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(frameTick:)];
        [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    }
    return self;
}

- (void)dealloc {
    [_displayLink invalidate];
    if (_titleImage != nil) CGImageRelease(_titleImage);
    if (_worldImage != nil) CGImageRelease(_worldImage);
}

- (void)ensureTitleImage {
    TH095Runtime &r = TH095Runtime::shared();
    if (_titleImage != nil || !r.assetsReady() || r.titleRgba().empty()) return;
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nullptr, r.titleRgba().data(), r.titleRgba().size(), nullptr);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    _titleImage = CGImageCreate(
        r.titleWidth(), r.titleHeight(), 8, 32, r.titleWidth() * 4,
        colorSpace, kCGImageAlphaLast | kCGBitmapByteOrderDefault,
        provider, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
}

- (void)ensureWorldImage {
    TH095Runtime &r = TH095Runtime::shared();
    if (_worldImage != nil || r.worldRgba().empty()) return;
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nullptr, r.worldRgba().data(), r.worldRgba().size(), nullptr);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    _worldImage = CGImageCreate(
        r.worldWidth(), r.worldHeight(), 8, 32, r.worldWidth() * 4,
        colorSpace, kCGImageAlphaLast | kCGBitmapByteOrderDefault,
        provider, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
}

- (void)frameTick:(CADisplayLink *)link {
    TH095Runtime::shared().tick(link.duration > 0 ? link.duration : 1.0 / 60.0);
    [self setNeedsDisplay];
}

- (void)layoutSubviews {
    [super layoutSubviews];
    _touchPoint = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
}

- (void)drawRect:(CGRect)rect {
    CGContextRef c = UIGraphicsGetCurrentContext();
    const CGFloat w = CGRectGetWidth(rect), h = CGRectGetHeight(rect);
    CGContextSetRGBFillColor(c, 0.025, 0.03, 0.08, 1); CGContextFillRect(c, rect);
    const BOOL portrait = h > w;
    CGFloat playH = portrait ? MIN(h * 0.58, w * 1.333) : MIN(h * 0.90, w * 0.75);
    CGFloat playW = portrait ? playH * 0.75 : playH * 1.333;
    CGRect play = CGRectMake((w - playW) * 0.5, portrait ? h * 0.16 : (h - playH) * 0.5, playW, playH);
    CGContextSetRGBFillColor(c, 0.02, 0.025, 0.07, 1); CGContextFillRect(c, play);
    CGContextSetRGBStrokeColor(c, 0.2, 0.35, 0.7, 1); CGContextStrokeRect(c, CGRectInset(play, 1, 1));
    [self ensureTitleImage];
    TH095Runtime &r = TH095Runtime::shared();
    [self ensureWorldImage];
    CGImageRef image = r.gameplayVisible() ? _worldImage : _titleImage;
    if (image != nil) {
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, 0, CGRectGetMaxY(play) + CGRectGetMinY(play));
        CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, play, image);
        CGContextRestoreGState(c);
    }
    CGContextSetRGBFillColor(c, 0.15, 0.22, 0.45, 1);
    CGContextFillEllipseInRect(c, CGRectMake(CGRectGetMinX(play) + playW * 0.70, CGRectGetMinY(play) + playH * 0.40, playW * 0.07, playW * 0.07));
    CGContextSetRGBFillColor(c, 0.9, 0.9, 1.0, 1);
    CGContextFillEllipseInRect(c, CGRectMake(CGRectGetMinX(play) + playW * r.playerX() - 9, CGRectGetMinY(play) + playH * r.playerY() - 9, 18, 18));
    if (_touchActive) {
        CGContextSetRGBStrokeColor(c, 0.5, 0.8, 1, 0.7); CGContextSetLineWidth(c, 3);
        CGContextStrokeEllipseInRect(c, CGRectMake(_touchPoint.x - 34, _touchPoint.y - 34, 68, 68));
    }
    NSDictionary *attrs = @{NSFontAttributeName:[UIFont systemFontOfSize:16], NSForegroundColorAttributeName:[UIColor whiteColor]};
    [[NSString stringWithFormat:@"TH095 iOS  |  %@  %@  Z %@  S %@", portrait ? @"竖屏战斗" : @"横屏", r.gameplayVisible()?@"Scene 1-1 资源":@"标题", r.zToggle()?@"ON":@"OFF", r.sToggle()?@"ON":@"OFF"] drawAtPoint:CGPointMake(18, 16) withAttributes:attrs];
    [[NSString stringWithFormat:@"资源：%@", r.assetsReady() ? @"title.anm 已加载" : [NSString stringWithUTF8String:r.assetStatus().c_str() ?: "未找到 th095.dat"]] drawAtPoint:CGPointMake(18, 40) withAttributes:attrs];
    [[NSString stringWithFormat:@"触摸移动   •   左下 Z   •   右下 S   •   %llu frames", (unsigned long long)r.frames()] drawAtPoint:CGPointMake(18, h - 35) withAttributes:attrs];
    if (r.settingsVisible()) {
        CGContextSetRGBFillColor(c, 0.03, 0.04, 0.10, 0.96); CGContextFillRect(c, CGRectMake(w * 0.22, h * 0.18, w * 0.56, h * 0.64));
        NSDictionary *menu = @{NSFontAttributeName:[UIFont systemFontOfSize:20], NSForegroundColorAttributeName:[UIColor whiteColor]};
        [@"TH095 设置" drawAtPoint:CGPointMake(w * 0.27, h * 0.24) withAttributes:menu];
        [[NSString stringWithFormat:@"Z 操作：%@（点按切换）", r.zMode()?@"Toggle":@"Hold"] drawAtPoint:CGPointMake(w * 0.27, h * 0.39) withAttributes:menu];
        [[NSString stringWithFormat:@"S 操作：%@（点按切换）", r.sMode()?@"Toggle":@"Hold"] drawAtPoint:CGPointMake(w * 0.27, h * 0.51) withAttributes:menu];
        [@"画面：横屏 60 FPS（模拟器验证）" drawAtPoint:CGPointMake(w * 0.27, h * 0.63) withAttributes:menu];
        [@"点右上角关闭" drawAtPoint:CGPointMake(w * 0.27, h * 0.73) withAttributes:menu];
    }
}

- (void)updateMove:(CGPoint)p {
    if (!_touchActive) return;
    CGFloat w = CGRectGetWidth(self.bounds), h = CGRectGetHeight(self.bounds);
    BOOL portrait = h > w;
    CGFloat playH = portrait ? MIN(h * 0.58, w * 1.333) : MIN(h * 0.90, w * 0.75);
    CGFloat playW = portrait ? playH * 0.75 : playH * 1.333;
    CGRect play = CGRectMake((w - playW) * 0.5, portrait ? h * 0.16 : (h - playH) * 0.5, playW, playH);
    CGFloat dx = p.x - CGRectGetMidX(play), dy = p.y - CGRectGetMidY(play);
    CGFloat scale = MAX(playW, playH) * 0.22;
    TH095Runtime::shared().setMove((float)MAX(-1.0, MIN(1.0, dx / scale)), (float)MAX(-1.0, MIN(1.0, dy / scale)));
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = touches.anyObject; CGPoint p = [touch locationInView:self]; _touchActive = YES; _touchPoint = p;
    if (p.x > CGRectGetWidth(self.bounds) * 0.86 && p.y < CGRectGetHeight(self.bounds) * 0.18) {
        TH095Runtime::shared().toggleSettings(); [self setNeedsDisplay]; return;
    }
    if (TH095Runtime::shared().settingsVisible()) {
        if (p.y > CGRectGetHeight(self.bounds) * 0.34 && p.y < CGRectGetHeight(self.bounds) * 0.47) {
            TH095Runtime::shared().setZMode(!TH095Runtime::shared().zMode());
        } else if (p.y >= CGRectGetHeight(self.bounds) * 0.47 && p.y < CGRectGetHeight(self.bounds) * 0.59) {
            TH095Runtime::shared().setSMode(!TH095Runtime::shared().sMode());
        }
        [self setNeedsDisplay]; return;
    }
    if (!TH095Runtime::shared().gameplayVisible() &&
        p.y > CGRectGetHeight(self.bounds) * 0.20 &&
        p.y < CGRectGetHeight(self.bounds) * 0.38) {
        TH095Runtime::shared().activatePrimaryMenu();
        [self setNeedsDisplay];
        return;
    }
    if (p.y > CGRectGetHeight(self.bounds) * 0.72) {
        if (p.x < CGRectGetWidth(self.bounds) * 0.28) { _zHeld = YES; TH095Runtime::shared().setActionZ(YES); }
        if (p.x > CGRectGetWidth(self.bounds) * 0.72) { _sHeld = YES; TH095Runtime::shared().setActionS(YES); }
    }
    [self updateMove:p];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self updateMove:[touches.anyObject locationInView:self]]; }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    _touchActive = NO; TH095Runtime::shared().setMove(0, 0);
    if (_zHeld) { _zHeld = NO; TH095Runtime::shared().setActionZ(NO); }
    if (_sHeld) { _sHeld = NO; TH095Runtime::shared().setActionS(NO); }
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self touchesEnded:touches withEvent:event]; }
@end
