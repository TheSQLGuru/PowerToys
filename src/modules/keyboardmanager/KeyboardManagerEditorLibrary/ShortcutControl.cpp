#include "pch.h"
#include "ShortcutControl.h"

#include <common/interop/shared_constants.h>

#include "KeyboardManagerState.h"
#include "KeyboardManagerEditorStrings.h"
#include "KeyDropDownControl.h"
#include "UIHelpers.h"
#include "EditorHelpers.h"
#include "EditorConstants.h"

//Both static members are initialized to null
HWND ShortcutControl::editShortcutsWindowHandle = nullptr;
KBMEditor::KeyboardManagerState* ShortcutControl::keyboardManagerState = nullptr;
// Initialized as new vector
RemapBuffer ShortcutControl::shortcutRemapBuffer;

ShortcutControl::ShortcutControl(StackPanel table, StackPanel row, const int colIndex, TextBox targetApp)
{
    shortcutDropDownVariableSizedWrapGrid = VariableSizedWrapGrid();
    typeShortcut = Button();
    shortcutControlLayout = StackPanel();
    const bool isHybridControl = colIndex == 1;

    // TODO: Check if there is a VariableSizedWrapGrid equivalent.
    // shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>().Spacing(EditorConstants::ShortcutTableDropDownSpacing);
    shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>().Orientation(Windows::UI::Xaml::Controls::Orientation::Horizontal);
    shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>().MaximumRowsOrColumns(3);

    typeShortcut.as<Button>().Content(winrt::box_value(GET_RESOURCE_STRING(IDS_TYPE_BUTTON)));
    typeShortcut.as<Button>().Width(EditorConstants::ShortcutTableDropDownWidth);
    typeShortcut.as<Button>().Click([&, table, row, colIndex, isHybridControl, targetApp](winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&) {
        keyboardManagerState->SetUIState(KBMEditor::KeyboardManagerUIState::DetectShortcutWindowActivated, editShortcutsWindowHandle);
        // Using the XamlRoot of the typeShortcut to get the root of the XAML host
        CreateDetectShortcutWindow(sender, sender.as<Button>().XamlRoot(), *keyboardManagerState, colIndex, table, keyDropDownControlObjects, row, targetApp, isHybridControl, false, editShortcutsWindowHandle, shortcutRemapBuffer);
    });

    // Set an accessible name for the type shortcut button
    typeShortcut.as<Button>().SetValue(Automation::AutomationProperties::NameProperty(), box_value(GET_RESOURCE_STRING(IDS_TYPE_BUTTON)));

    shortcutControlLayout.as<StackPanel>().Spacing(EditorConstants::ShortcutTableDropDownSpacing);

    keyComboAndSelectStackPanel = StackPanel();
    keyComboAndSelectStackPanel.as<StackPanel>().Orientation(Windows::UI::Xaml::Controls::Orientation::Horizontal);
    keyComboAndSelectStackPanel.as<StackPanel>().Spacing(EditorConstants::ShortcutTableDropDownSpacing);

    keyComboAndSelectStackPanel.as<StackPanel>().Children().Append(typeShortcut.as<Button>());
    shortcutControlLayout.as<StackPanel>().Children().InsertAt(0, keyComboAndSelectStackPanel.as<StackPanel>());

    shortcutControlLayout.as<StackPanel>().Children().Append(shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>());
    KeyDropDownControl::AddDropDown(table, row, shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>(), colIndex, shortcutRemapBuffer, keyDropDownControlObjects, targetApp, isHybridControl, false);
    try
    {
        // If a layout update has been triggered by other methods (e.g.: adapting to zoom level), this may throw an exception.
        shortcutControlLayout.as<StackPanel>().UpdateLayout();
    }
    catch (...)
    {
    }
}

// Function to set the accessible name of the target App text box
void ShortcutControl::SetAccessibleNameForTextBox(TextBox targetAppTextBox, int rowIndex)
{
    // To set the accessible name of the target App text box by adding the string `All Apps` if the text box is empty, if not the application name is read by narrator.
    std::wstring targetAppTextBoxAccessibleName = GET_RESOURCE_STRING(IDS_AUTOMATIONPROPERTIES_ROW) + std::to_wstring(rowIndex) + L", " + GET_RESOURCE_STRING(IDS_EDITSHORTCUTS_TARGETAPPHEADER);
    if (targetAppTextBox.Text() == L"")
    {
        targetAppTextBoxAccessibleName += GET_RESOURCE_STRING(IDS_EDITSHORTCUTS_ALLAPPS);
    }

    targetAppTextBox.SetValue(Automation::AutomationProperties::NameProperty(), box_value(targetAppTextBoxAccessibleName));
}

// Function to set the accessible names for all the controls in a row
void ShortcutControl::UpdateAccessibleNames(StackPanel sourceColumn, StackPanel mappedToColumn, TextBox targetAppTextBox, Button deleteButton, int rowIndex)
{
    sourceColumn.SetValue(Automation::AutomationProperties::NameProperty(), box_value(GET_RESOURCE_STRING(IDS_AUTOMATIONPROPERTIES_ROW) + std::to_wstring(rowIndex) + L", " + GET_RESOURCE_STRING(IDS_EDITSHORTCUTS_SOURCEHEADER)));
    mappedToColumn.SetValue(Automation::AutomationProperties::NameProperty(), box_value(GET_RESOURCE_STRING(IDS_AUTOMATIONPROPERTIES_ROW) + std::to_wstring(rowIndex) + L", " + GET_RESOURCE_STRING(IDS_EDITSHORTCUTS_TARGETHEADER)));
    ShortcutControl::SetAccessibleNameForTextBox(targetAppTextBox, rowIndex);
    deleteButton.SetValue(Automation::AutomationProperties::NameProperty(), box_value(GET_RESOURCE_STRING(IDS_AUTOMATIONPROPERTIES_ROW) + std::to_wstring(rowIndex) + L", " + GET_RESOURCE_STRING(IDS_DELETE_REMAPPING_BUTTON)));
}

// Function to add a new row to the shortcut table. If the originalKeys and newKeys args are provided, then the displayed shortcuts are set to those values.
void ShortcutControl::AddNewShortcutControlRow(StackPanel& parent, std::vector<std::vector<std::unique_ptr<ShortcutControl>>>& keyboardRemapControlObjects, const Shortcut& originalKeys, const KeyShortcutTextUnion& newKeys, const std::wstring& targetAppName)
{
    // Textbox for target application
    TextBox targetAppTextBox;

    // Create new ShortcutControl objects dynamically so that we does not get destructed
    std::vector<std::unique_ptr<ShortcutControl>> newrow;
    StackPanel row = StackPanel();
    parent.Children().Append(row);
    newrow.emplace_back(std::make_unique<ShortcutControl>(parent, row, 0, targetAppTextBox));
    newrow.emplace_back(std::make_unique<ShortcutControl>(parent, row, 1, targetAppTextBox));
    keyboardRemapControlObjects.push_back(std::move(newrow));

    row.Padding({ 10, 15, 10, 5 });
    row.Margin({ 0, 0, 0, 2 });
    row.Orientation(Orientation::Horizontal);
    row.Background(Application::Current().Resources().Lookup(box_value(L"CardBackgroundFillColorDefaultBrush")).as<Media::Brush>());
    row.BorderBrush(Application::Current().Resources().Lookup(box_value(L"CardStrokeColorDefaultBrush")).as<Media::Brush>());
    row.BorderThickness({ 0, 1, 0, 1 });

    // ShortcutControl for the original shortcut
    auto origin = keyboardRemapControlObjects.back()[0]->GetShortcutControl();
    origin.Width(EditorConstants::ShortcutOriginColumnWidth);
    row.Children().Append(origin);

    // Arrow icon
    SymbolIcon arrowIcon(Symbol::Forward);
    arrowIcon.VerticalAlignment(VerticalAlignment::Center);
    arrowIcon.HorizontalAlignment(HorizontalAlignment::Center);
    auto arrowIconContainer = UIHelpers::GetWrapped(arrowIcon, EditorConstants::ShortcutArrowColumnWidth).as<StackPanel>();
    arrowIconContainer.Orientation(Orientation::Vertical);
    arrowIconContainer.VerticalAlignment(VerticalAlignment::Center);
    arrowIconContainer.Margin({ 0, 0, 0, 10 });
    row.Children().Append(arrowIconContainer);

    // ShortcutControl for the new shortcut
    auto target = keyboardRemapControlObjects.back()[1]->GetShortcutControl();
    target.Width(EditorConstants::ShortcutTargetColumnWidth);

    auto typeCombo = ComboBox();
    typeCombo.Width(EditorConstants::RemapTableDropDownWidth);
    typeCombo.Items().Append(winrt::box_value(KeyboardManagerEditorStrings::MappingTypeShortcut()));
    typeCombo.Items().Append(winrt::box_value(KeyboardManagerEditorStrings::MappingTypeText()));
    auto controlStackPanel = keyboardRemapControlObjects.back()[1]->shortcutControlLayout.as<StackPanel>();
    auto firstLineStackPanel = keyboardRemapControlObjects.back()[1]->keyComboAndSelectStackPanel.as<StackPanel>();
    firstLineStackPanel.Children().InsertAt(0, typeCombo);

    auto textInput = TextBox();
    auto textInputMargin = Windows::UI::Xaml::Thickness();
    textInputMargin.Top = -EditorConstants::ShortcutTableDropDownSpacing;
    textInputMargin.Bottom = EditorConstants::ShortcutTableDropDownSpacing; // compensate for a collapsed UIElement
    textInput.Margin(textInputMargin);

    textInput.AcceptsReturn(false);
    textInput.Visibility(Visibility::Collapsed);
    textInput.Width(EditorConstants::TableDropDownHeight);
    controlStackPanel.Children().Append(textInput);
    textInput.HorizontalAlignment(HorizontalAlignment::Left);
    textInput.TextChanged([parent, row](winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const& e) mutable {
        auto textbox = sender.as<TextBox>();
        auto text = textbox.Text();
        uint32_t rowIndex = -1;

        if (!parent.Children().IndexOf(row, rowIndex))
        {
            return;
        }

        shortcutRemapBuffer[rowIndex].first[1] = text.c_str();
    });

    auto grid = keyboardRemapControlObjects.back()[1]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>();
    auto gridMargin = Windows::UI::Xaml::Thickness();
    gridMargin.Bottom = -EditorConstants::ShortcutTableDropDownSpacing; // compensate for a collapsed textInput
    grid.Margin(gridMargin);
    auto button = keyboardRemapControlObjects.back()[1]->typeShortcut.as<Button>();

    typeCombo.SelectionChanged([typeCombo, grid, button, textInput](winrt::Windows::Foundation::IInspectable const&, SelectionChangedEventArgs const&) {
        const bool textSelected = typeCombo.SelectedIndex() == 1;

        const auto shortcutInputVisibility = textSelected ? Visibility::Collapsed : Visibility::Visible;

        grid.Visibility(shortcutInputVisibility);
        button.Visibility(shortcutInputVisibility);

        const auto textInputVisibility = textSelected ? Visibility::Visible : Visibility::Collapsed;
        textInput.Visibility(textInputVisibility);
    });

    const bool textSelected = newKeys.index() == 2;
    typeCombo.SelectedIndex(textSelected);

    row.Children().Append(target);

    targetAppTextBox.Width(EditorConstants::ShortcutTableDropDownWidth);
    targetAppTextBox.PlaceholderText(KeyboardManagerEditorStrings::DefaultAppName());
    targetAppTextBox.Text(targetAppName);
    targetAppTextBox.Margin({ 0, 0, 0, 10 });

    // GotFocus handler will be called whenever the user tabs into or clicks on the textbox
    targetAppTextBox.GotFocus([targetAppTextBox](auto const& sender, auto const& e) {
        // Select all text for accessible purpose
        targetAppTextBox.SelectAll();
    });

    // LostFocus handler will be called whenever text is updated by a user and then they click something else or tab to another control. Does not get called if Text is updated while the TextBox isn't in focus (i.e. from code)
    targetAppTextBox.LostFocus([&keyboardRemapControlObjects, parent, row, targetAppTextBox, typeCombo, textInput](auto const& sender, auto const& e) {
        // Get index of targetAppTextBox button
        uint32_t rowIndex;
        if (!parent.Children().IndexOf(row, rowIndex))
        {
            return;
        }

        // rowIndex could be out of bounds if the row got deleted after LostFocus handler was invoked. In this case it should return
        if (rowIndex >= keyboardRemapControlObjects.size())
        {
            return;
        }

        // Validate both set of drop downs
        KeyDropDownControl::ValidateShortcutFromDropDownList(parent, row, keyboardRemapControlObjects[rowIndex][0]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>(), 0, ShortcutControl::shortcutRemapBuffer, keyboardRemapControlObjects[rowIndex][0]->keyDropDownControlObjects, targetAppTextBox, false, false);
        KeyDropDownControl::ValidateShortcutFromDropDownList(parent, row, keyboardRemapControlObjects[rowIndex][1]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>(), 1, ShortcutControl::shortcutRemapBuffer, keyboardRemapControlObjects[rowIndex][1]->keyDropDownControlObjects, targetAppTextBox, true, false);

        // Reset the buffer based on the selected drop down items
        std::get<Shortcut>(shortcutRemapBuffer[rowIndex].first[0]).SetKeyCodes(KeyDropDownControl::GetSelectedCodesFromStackPanel(keyboardRemapControlObjects[rowIndex][0]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>()));
        // second column is a hybrid column

        const bool textSelected = typeCombo.SelectedIndex() == 1;
        if (textSelected)
        {
            shortcutRemapBuffer[rowIndex].first[1] = textInput.Text().c_str();
        }
        else
        {
            std::vector<int32_t> selectedKeyCodes = KeyDropDownControl::GetSelectedCodesFromStackPanel(keyboardRemapControlObjects[rowIndex][1]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>());

            // If exactly one key is selected consider it to be a key remap
            if (selectedKeyCodes.size() == 1)
            {
                shortcutRemapBuffer[rowIndex].first[1] = (DWORD)selectedKeyCodes[0];
            }
            else
            {
                Shortcut tempShortcut;
                tempShortcut.SetKeyCodes(selectedKeyCodes);
                // Assign instead of setting the value in the buffer since the previous value may not be a Shortcut
                shortcutRemapBuffer[rowIndex].first[1] = tempShortcut;
            }
        }
        std::wstring newText = targetAppTextBox.Text().c_str();
        std::wstring lowercaseDefAppName = KeyboardManagerEditorStrings::DefaultAppName();
        std::transform(newText.begin(), newText.end(), newText.begin(), towlower);
        std::transform(lowercaseDefAppName.begin(), lowercaseDefAppName.end(), lowercaseDefAppName.begin(), towlower);
        if (newText == lowercaseDefAppName)
        {
            shortcutRemapBuffer[rowIndex].second = L"";
        }
        else
        {
            shortcutRemapBuffer[rowIndex].second = targetAppTextBox.Text().c_str();
        }

        // To set the accessible name of the target app text box when focus is lost
        ShortcutControl::SetAccessibleNameForTextBox(targetAppTextBox, rowIndex + 1);
    });

    // We need two containers in order to align it horizontally and vertically
    StackPanel targetAppHorizontal = UIHelpers::GetWrapped(targetAppTextBox, EditorConstants::TableTargetAppColWidth).as<StackPanel>();
    targetAppHorizontal.Orientation(Orientation::Horizontal);
    targetAppHorizontal.HorizontalAlignment(HorizontalAlignment::Left);
    StackPanel targetAppContainer = UIHelpers::GetWrapped(targetAppHorizontal, EditorConstants::TableTargetAppColWidth).as<StackPanel>();
    targetAppContainer.Orientation(Orientation::Vertical);
    targetAppContainer.VerticalAlignment(VerticalAlignment::Center);
    row.Children().Append(targetAppContainer);

    // Delete row button
    Windows::UI::Xaml::Controls::Button deleteShortcut;
    deleteShortcut.Content(SymbolIcon(Symbol::Delete));
    deleteShortcut.Background(Media::SolidColorBrush(Colors::Transparent()));
    deleteShortcut.HorizontalAlignment(HorizontalAlignment::Center);
    deleteShortcut.Margin({ 0, 0, 0, 10 });
    deleteShortcut.Click([&, parent, row, deleteShortcut](winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&) {
        Button currentButton = sender.as<Button>();
        uint32_t rowIndex;
        // Get index of delete button
        UIElementCollection children = parent.Children();
        bool indexFound = children.IndexOf(row, rowIndex);

        // IndexOf could fail if the row got deleted and the button handler was invoked twice. In this case it should return
        if (!indexFound)
        {
            return;
        }

        for (uint32_t i = rowIndex + 1; i < children.Size(); i++)
        {
            StackPanel row = children.GetAt(i).as<StackPanel>();
            StackPanel sourceCol = row.Children().GetAt(0).as<StackPanel>();
            StackPanel targetCol = row.Children().GetAt(2).as<StackPanel>();
            TextBox targetApp = row.Children().GetAt(3).as<StackPanel>().Children().GetAt(0).as<StackPanel>().Children().GetAt(0).as<TextBox>();
            Button delButton = row.Children().GetAt(4).as<StackPanel>().Children().GetAt(0).as<Button>();
            UpdateAccessibleNames(sourceCol, targetCol, targetApp, delButton, i);
        }

        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(deleteShortcut) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                GET_RESOURCE_STRING(IDS_DELETE_REMAPPING_EVENT),
                L"ShortcutRemappingDeletedNotificationEvent" /* unique name for this notification category */);
        }

        children.RemoveAt(rowIndex);
        shortcutRemapBuffer.erase(shortcutRemapBuffer.begin() + rowIndex);
        // delete the SingleKeyRemapControl objects so that they get destructed
        keyboardRemapControlObjects.erase(keyboardRemapControlObjects.begin() + rowIndex);
    });

    // To set the accessible name of the delete button
    deleteShortcut.SetValue(Automation::AutomationProperties::NameProperty(), box_value(GET_RESOURCE_STRING(IDS_DELETE_REMAPPING_BUTTON)));

    // Add tooltip for delete button which would appear on hover
    ToolTip deleteShortcuttoolTip;
    deleteShortcuttoolTip.Content(box_value(GET_RESOURCE_STRING(IDS_DELETE_REMAPPING_BUTTON)));
    ToolTipService::SetToolTip(deleteShortcut, deleteShortcuttoolTip);

    StackPanel deleteShortcutContainer = StackPanel();
    deleteShortcutContainer.Children().Append(deleteShortcut);
    deleteShortcutContainer.Orientation(Orientation::Vertical);
    deleteShortcutContainer.VerticalAlignment(VerticalAlignment::Center);
    row.Children().Append(deleteShortcutContainer);

    // Set accessible names
    UpdateAccessibleNames(keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][0]->GetShortcutControl(), keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][1]->GetShortcutControl(), targetAppTextBox, deleteShortcut, static_cast<int>(keyboardRemapControlObjects.size()));

    // Set the shortcut text if the two vectors are not empty (i.e. default args)
    if (EditorHelpers::IsValidShortcut(originalKeys) && !(newKeys.index() == 0 && std::get<DWORD>(newKeys) == NULL) && !(newKeys.index() == 1 && !EditorHelpers::IsValidShortcut(std::get<Shortcut>(newKeys))))
    {
        // change to load app name
        shortcutRemapBuffer.push_back(std::make_pair<RemapBufferItem, std::wstring>(RemapBufferItem{ Shortcut(), Shortcut() }, std::wstring(targetAppName)));
        KeyDropDownControl::AddShortcutToControl(originalKeys, parent, keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][0]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>(), *keyboardManagerState, 0, keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][0]->keyDropDownControlObjects, shortcutRemapBuffer, row, targetAppTextBox, false, false);

        if (newKeys.index() == 0)
        {
            keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][1]->keyDropDownControlObjects[0]->SetSelectedValue(std::to_wstring(std::get<DWORD>(newKeys)));
        }
        else if (newKeys.index() == 1)
        {
            KeyDropDownControl::AddShortcutToControl(std::get<Shortcut>(newKeys), parent, keyboardRemapControlObjects.back()[1]->shortcutDropDownVariableSizedWrapGrid.as<VariableSizedWrapGrid>(), *keyboardManagerState, 1, keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][1]->keyDropDownControlObjects, shortcutRemapBuffer, row, targetAppTextBox, true, false);
        }
        else if (newKeys.index() == 2)
        {
            shortcutRemapBuffer.back().first[1] = std::get<std::wstring>(newKeys);
            const auto& remapControl = keyboardRemapControlObjects[keyboardRemapControlObjects.size() - 1][1];
            const auto& controlChildren = remapControl->GetShortcutControl().Children();
            const auto& topLineChildren = controlChildren.GetAt(0).as<StackPanel>();
            topLineChildren.Children().GetAt(0).as<ComboBox>().SelectedIndex(1);
            controlChildren.GetAt(2).as<TextBox>().Text(std::get<std::wstring>(newKeys));
        }
    }
    else
    {
        // Initialize both shortcuts as empty shortcuts
        shortcutRemapBuffer.push_back(std::make_pair<RemapBufferItem, std::wstring>(RemapBufferItem{ Shortcut(), Shortcut() }, std::wstring(targetAppName)));
    }
}

// Function to return the stack panel element of the ShortcutControl. This is the externally visible UI element which can be used to add it to other layouts
StackPanel ShortcutControl::GetShortcutControl()
{
    return shortcutControlLayout.as<StackPanel>();
}

// Function to create the detect shortcut UI window
void ShortcutControl::CreateDetectShortcutWindow(winrt::Windows::Foundation::IInspectable const& sender, XamlRoot xamlRoot, KBMEditor::KeyboardManagerState& keyboardManagerState, const int colIndex, StackPanel table, std::vector<std::unique_ptr<KeyDropDownControl>>& keyDropDownControlObjects, StackPanel row, TextBox targetApp, bool isHybridControl, bool isSingleKeyWindow, HWND parentWindow, RemapBuffer& remapBuffer)
{
    // ContentDialog for detecting shortcuts. This is the parent UI element.
    ContentDialog detectShortcutBox;

    // ContentDialog requires manually setting the XamlRoot (https://learn.microsoft.com/uwp/api/windows.ui.xaml.controls.contentdialog#contentdialog-in-appwindow-or-xaml-islands)
    detectShortcutBox.XamlRoot(xamlRoot);
    detectShortcutBox.Title(box_value(GET_RESOURCE_STRING(IDS_TYPESHORTCUT_TITLE)));

    // Get the parent linked stack panel for the "Type shortcut" button that was clicked
    VariableSizedWrapGrid linkedShortcutVariableSizedWrapGrid = UIHelpers::GetSiblingElement(sender.as<FrameworkElement>().Parent()).as<VariableSizedWrapGrid>();

    auto unregisterKeys = [&keyboardManagerState]() {
        keyboardManagerState.ClearRegisteredKeyDelays();
    };

    auto selectDetectedShortcutAndResetKeys = [&keyboardManagerState](DWORD key) {
        keyboardManagerState.SelectDetectedShortcut(key);
        keyboardManagerState.ResetDetectedShortcutKey(key);
    };

    auto onPressEnter = [linkedShortcutVariableSizedWrapGrid,
                         detectShortcutBox,
                         &keyboardManagerState,
                         unregisterKeys,
                         colIndex,
                         table,
                         targetApp,
                         &keyDropDownControlObjects,
                         row,
                         isHybridControl,
                         isSingleKeyWindow,
                         &remapBuffer] {
        // Save the detected shortcut in the linked text block
        Shortcut detectedShortcutKeys = keyboardManagerState.GetDetectedShortcut();

        if (!detectedShortcutKeys.IsEmpty())
        {
            // The shortcut buffer gets set in this function
            KeyDropDownControl::AddShortcutToControl(detectedShortcutKeys, table, linkedShortcutVariableSizedWrapGrid, keyboardManagerState, colIndex, keyDropDownControlObjects, remapBuffer, row, targetApp, isHybridControl, isSingleKeyWindow);
        }
        // Hide the type shortcut UI
        detectShortcutBox.Hide();
    };

    auto onReleaseEnter = [&keyboardManagerState,
                           unregisterKeys,
                           isSingleKeyWindow,
                           parentWindow] {
        // Reset the keyboard manager UI state
        keyboardManagerState.ResetUIState();
        if (isSingleKeyWindow)
        {
            // Revert UI state back to Edit Keyboard window
            keyboardManagerState.SetUIState(KBMEditor::KeyboardManagerUIState::EditKeyboardWindowActivated, parentWindow);
        }
        else
        {
            // Revert UI state back to Edit Shortcut window
            keyboardManagerState.SetUIState(KBMEditor::KeyboardManagerUIState::EditShortcutsWindowActivated, parentWindow);
        }

        unregisterKeys();
    };

    auto onAccept = [onPressEnter,
                     onReleaseEnter] {
        onPressEnter();
        onReleaseEnter();
    };

    // OK button
    detectShortcutBox.DefaultButton(ContentDialogButton::Primary);
    detectShortcutBox.PrimaryButtonText(GET_RESOURCE_STRING(IDS_OK_BUTTON));
    detectShortcutBox.PrimaryButtonClick([onAccept](winrt::Windows::Foundation::IInspectable const& sender, ContentDialogButtonClickEventArgs const& args) {
        // Cancel default dialog events
        args.Cancel(true);

        onAccept();
    });

    // NOTE: UnregisterKeys should never be called on the DelayThread, as it will re-enter the mutex. To avoid this it is run on the dispatcher thread
    keyboardManagerState.RegisterKeyDelay(
        VK_RETURN,
        selectDetectedShortcutAndResetKeys,
        [onPressEnter, detectShortcutBox](DWORD) {
            detectShortcutBox.Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [onPressEnter] {
                    onPressEnter();
                });
        },
        [onReleaseEnter, detectShortcutBox](DWORD) {
            detectShortcutBox.Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [onReleaseEnter]() {
                    onReleaseEnter();
                });
        });

    auto onCancel = [&keyboardManagerState,
                     detectShortcutBox,
                     unregisterKeys,
                     isSingleKeyWindow,
                     parentWindow] {
        detectShortcutBox.Hide();

        // Reset the keyboard manager UI state
        keyboardManagerState.ResetUIState();
        if (isSingleKeyWindow)
        {
            // Revert UI state back to Edit Keyboard window
            keyboardManagerState.SetUIState(KBMEditor::KeyboardManagerUIState::EditKeyboardWindowActivated, parentWindow);
        }
        else
        {
            // Revert UI state back to Edit Shortcut window
            keyboardManagerState.SetUIState(KBMEditor::KeyboardManagerUIState::EditShortcutsWindowActivated, parentWindow);
        }
        unregisterKeys();
    };

    // Cancel button
    detectShortcutBox.CloseButtonText(GET_RESOURCE_STRING(IDS_CANCEL_BUTTON));
    detectShortcutBox.CloseButtonClick([onCancel](winrt::Windows::Foundation::IInspectable const& sender, ContentDialogButtonClickEventArgs const& args) {
        // Cancel default dialog events
        args.Cancel(true);

        onCancel();
    });

    // NOTE: UnregisterKeys should never be called on the DelayThread, as it will re-enter the mutex. To avoid this it is run on the dispatcher thread
    keyboardManagerState.RegisterKeyDelay(
        VK_ESCAPE,
        selectDetectedShortcutAndResetKeys,
        [onCancel, detectShortcutBox](DWORD) {
            detectShortcutBox.Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [onCancel] {
                    onCancel();
                });
        },
        nullptr);

    // StackPanel parent for the displayed text in the dialog
    Windows::UI::Xaml::Controls::StackPanel stackPanel;
    detectShortcutBox.Content(stackPanel);

    // Header textblock
    TextBlock text;
    text.Text(GET_RESOURCE_STRING(IDS_TYPESHORTCUT_HEADER));
    text.Margin({ 0, 0, 0, 10 });
    stackPanel.Children().Append(text);

    // Target StackPanel to place the selected key - first row (for 1-3 keys)
    Windows::UI::Xaml::Controls::StackPanel keyStackPanel1;
    keyStackPanel1.Orientation(Orientation::Horizontal);
    stackPanel.Children().Append(keyStackPanel1);

    // Target StackPanel to place the selected key - second row (for 4-5 keys)
    Windows::UI::Xaml::Controls::StackPanel keyStackPanel2;
    keyStackPanel2.Orientation(Orientation::Horizontal);
    keyStackPanel2.Margin({ 0, 20, 0, 0 });
    keyStackPanel2.Visibility(Visibility::Collapsed);
    stackPanel.Children().Append(keyStackPanel2);

    TextBlock holdEscInfo;
    holdEscInfo.Text(GET_RESOURCE_STRING(IDS_TYPE_HOLDESC));
    holdEscInfo.FontSize(12);
    holdEscInfo.Margin({ 0, 20, 0, 0 });
    stackPanel.Children().Append(holdEscInfo);

    TextBlock holdEnterInfo;
    holdEnterInfo.Text(GET_RESOURCE_STRING(IDS_TYPE_HOLDENTER));
    holdEnterInfo.FontSize(12);
    holdEnterInfo.Margin({ 0, 0, 0, 0 });
    stackPanel.Children().Append(holdEnterInfo);

    try
    {
        // If a layout update has been triggered by other methods (e.g.: adapting to zoom level), this may throw an exception.
        stackPanel.UpdateLayout();
    }
    catch (...)
    {
    }

    // Configure the keyboardManagerState to store the UI information.
    keyboardManagerState.ConfigureDetectShortcutUI(keyStackPanel1, keyStackPanel2);

    // Show the dialog
    detectShortcutBox.ShowAsync();
}
