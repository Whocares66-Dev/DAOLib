// The game's screenshots, as the screenshot key takes them: a plugin told before and after, so
// that what it draws of its own (a screen of its own, dao/Gui.h) is kept out of the picture.
//
//   dao::screenshot::OnScreenshot([] { dao::gui::HideScreen(L"MyPanel"); },
//                                 [] { dao::gui::ShowScreen(L"MyPanel"); });
#pragma once

#include <functional>

namespace dao::screenshot
{

// Calls `before` when the game is about to take a screenshot, and `after` once its file is
// written.
// @param before runs on the game thread when the screenshot is asked for; the picture is taken
//  two frames later, so what it hides is not in it
// @param after runs on the game thread, the frame after the file is written, or a few seconds on
//  if it never is
//
// Each plugin that asks is told. A screenshot asked for with a name of its own (the message's
// strings not empty) is taken at once, unheld, and no one is told.
void OnScreenshot(std::function<void()> before, std::function<void()> after);

} // namespace dao::screenshot
