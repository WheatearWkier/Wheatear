#include "wtpch.h"
#include "VisualNovelInputService.h"

#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"

namespace Wheatear::VisualNovelInputService {

    InputSnapshot Sample()
    {
        InputSnapshot snapshot;
        snapshot.PrimaryMousePressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
        snapshot.AdvanceActionPressed = InputBindingService::IsActionDown("vn.advance");
        snapshot.AutoPressed = InputBindingService::IsActionDown("vn.auto");
        snapshot.HistoryPressed = InputBindingService::IsActionDown("vn.history");
        snapshot.SavePressed = InputBindingService::IsActionDown("vn.save");
        snapshot.LoadPressed = InputBindingService::IsActionDown("vn.load");

        for (size_t i = 0; i < snapshot.ChoicePressed.size(); ++i)
            snapshot.ChoicePressed[i] = Input::IsKeyPressed(WT_KEY_1 + static_cast<int>(i));

        return snapshot;
    }

} // namespace Wheatear::VisualNovelInputService
