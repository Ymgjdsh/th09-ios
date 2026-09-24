import XCTest
import UIKit

final class TH095UiRegressionTests: XCTestCase {
    private let game = XCUIApplication(bundleIdentifier: "com.th095.reconstruction")

    override func setUpWithError() throws {
        continueAfterFailure = false
    }

    private func settle(_ seconds: TimeInterval = 10) {
        let settled = expectation(description: "Allow the game to render after orientation or input")
        DispatchQueue.main.asyncAfter(deadline: .now() + seconds) { settled.fulfill() }
        wait(for: [settled], timeout: seconds + 15)
    }

    private func launchGame() {
        // This runner owns no game data. Launch the separately installed app.
        game.launch()
        XCTAssertTrue(game.wait(for: .runningForeground, timeout: 20))
        settle()
    }

    @discardableResult
    private func capture(_ name: String, landscape: Bool) -> CGSize {
        XCTAssertEqual(game.state, .runningForeground, "Game exited before screenshot \(name)")
        let screenshot = XCUIScreen.main.screenshot()
        let attachment = XCTAttachment(screenshot: screenshot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)

        let size = screenshot.image.size
        print("TH095_UI_CAPTURE name=\(name) screenshot=\(size.width)x\(size.height) accessibilityFrame=\(game.frame) orientation=\(XCUIDevice.shared.orientation.rawValue)")
        if landscape {
            XCTAssertGreaterThan(size.width, size.height, "Screenshot must really be landscape")
            // SDL accessibility can retain a portrait app.frame; the oriented screenshot and game drawable log are authoritative.
        } else {
            XCTAssertGreaterThan(size.height, size.width, "Screenshot must really be portrait")
            // Verify the drawable dimensions in startup.log alongside this screenshot.
        }
        return size
    }

    private func rotate(_ orientation: UIDeviceOrientation, name: String) {
        XCUIDevice.shared.orientation = orientation
        settle()
        capture(name, landscape: orientation.isLandscape)
    }

    private func tapNormalized(x: CGFloat, y: CGFloat) {
        XCTAssertTrue((0...1).contains(x) && (0...1).contains(y))
        let size = XCUIScreen.main.screenshot().image.size
        game.coordinate(withNormalizedOffset: CGVector(dx: 0, dy: 0)).withOffset(CGVector(dx: x * size.width, dy: y * size.height)).tap()
    }

    // Mirror the public presentation geometry in UIKit points, so a source
    // coordinate is useful on Retina devices without assuming pixel density.
    private func tapGamePoint(x: CGFloat, y: CGFloat, portraitBattle: Bool = false) {
        let frame = CGRect(origin: .zero, size: XCUIScreen.main.screenshot().image.size)
        let w = frame.width, h = frame.height
        let cropped = portraitBattle && h > w
        let sourceW: CGFloat = cropped ? 384 : 640
        let sourceH: CGFloat = cropped ? 448 : 480
        var viewportW: CGFloat
        var viewportH: CGFloat
        var viewportY: CGFloat
        if h > w {
            let unit = min(1.6, min(w, h) / 390)
            let topInset = CGFloat(Int(82 * unit))
            let availableH = max(1, h - topInset - CGFloat(Int(204 * unit)))
            viewportH = min(w * sourceH / sourceW, availableH)
            viewportW = viewportH * sourceW / sourceH
            viewportY = topInset + (availableH - viewportH) / 2
        } else {
            let scale = min(w / sourceW, h / sourceH)
            viewportW = sourceW * scale
            viewportH = sourceH * scale
            viewportY = (h - viewportH) / 2
        }
        let viewportX = (w - viewportW) / 2
        let px = viewportX + (x - (cropped ? 128 : 0)) * viewportW / sourceW
        let py = viewportY + (y - (cropped ? 16 : 0)) * viewportH / sourceH
        print("TH095_UI_TAP source=\(x),\(y) normalized=\(px/w),\(py/h)")
        tapNormalized(x: px / w, y: py / h)
    }

    private func tapPhotoButton() {
        let frame = CGRect(origin: .zero, size: XCUIScreen.main.screenshot().image.size)
        let unit = min(1.6, min(frame.width, frame.height) / 390)
        // The Z circle's centre is (w - 48u, h - 130u).
        tapNormalized(x: (frame.width - 48 * unit) / frame.width,
                      y: (frame.height - 130 * unit) / frame.height)
    }

    func testRotateLandscapePortrait() {
        launchGame()
        rotate(.landscapeLeft, name: "title-landscape")
        rotate(.portrait, name: "title-portrait")
    }

    func testBattleRotateLandscapePortrait() {
        launchGame()
        rotate(.landscapeLeft, name: "before-start-landscape")
        tapGamePoint(x: 150, y: 140)
        settle()
        capture("scene-select-landscape", landscape: true)
        tapPhotoButton()
        settle()
        capture("battle-landscape", landscape: true)
        rotate(.portrait, name: "battle-portrait")
        rotate(.landscapeRight, name: "battle-landscape-right")
    }

    func testReplayReturnThenBattle() {
        launchGame()
        rotate(.landscapeLeft, name: "before-replay-landscape")
        tapGamePoint(x: 150, y: 178)
        settle()
        capture("replay-landscape", landscape: true)
        let frame = CGRect(origin: .zero, size: XCUIScreen.main.screenshot().image.size)
        let unit = min(1.6, min(frame.width, frame.height) / 390)
        // The X circle's centre is (w - 105u, h - 73u).
        tapNormalized(x: (frame.width - 105 * unit) / frame.width,
                      y: (frame.height - 73 * unit) / frame.height)
        settle()
        capture("returned-title-landscape", landscape: true)
        tapGamePoint(x: 150, y: 140)
        settle()
        capture("after-replay-select", landscape: true)
        tapPhotoButton()
        settle()
        capture("after-replay-battle", landscape: true)
    }

    func testPortraitBattleAndSettings() {
        launchGame()
        rotate(.portrait, name: "title-portrait")
        tapGamePoint(x: 150, y: 140)
        settle()
        tapPhotoButton()
        settle(2)
        capture("active-battle-portrait", landscape: false)
        let size = XCUIScreen.main.screenshot().image.size
        let unit = min(1.6, min(size.width, size.height) / 390)
        tapNormalized(x: (size.width-81*unit)/size.width, y: 55*unit/size.height)
        settle(2)
        capture("paused-battle-portrait", landscape: false)
        tapNormalized(x: (size.width-35*unit)/size.width, y: 55*unit/size.height)
        settle(2)
        capture("touch-settings", landscape: false)
        // Exercise both hold/toggle settings, then restore their initial state.
        let rowHeight = min(45*unit,size.height*0.11)
        let top = (size.height-rowHeight*5.8)/2
        for row in 0...1 {
            for _ in 0...1 {
                tapNormalized(x: 0.5, y: (top+rowHeight*(CGFloat(row)*1.2+0.5))/size.height)
                settle(0.3)
            }
        }
        tapNormalized(x: 0.5, y: (top+rowHeight*5.3)/size.height)
        settle(1)
        capture("settings-return", landscape: false)
    }
}
