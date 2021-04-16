#include "ExternalEditTextInputs.h"

#include "ExternalEditTextInputsSettings.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "ISettingsModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/InteractiveProcess.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Notifications/SNotificationList.h"

IMPLEMENT_MODULE(FExternalEditTextInputsModule, ExternalEditTextInputs)

#define LOCTEXT_NAMESPACE "ExternalEditTextInputsModule"

void ShowNotification(
	const FText& Text, const SNotificationItem::ECompletionState CompletionState, const float ExpireDuration = 5.f)
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = ExpireDuration;
	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationItem->SetCompletionState(CompletionState);
	NotificationItem->ExpireAndFadeout();
}

void ShowNotification(const FText& Text, const bool bSuccess, const float ExpireDuration = 15.f)
{
	const SNotificationItem::ECompletionState CompletionState =
		bSuccess ? SNotificationItem::ECompletionState::CS_Success : SNotificationItem::ECompletionState::CS_Fail;
	ShowNotification(Text, CompletionState, ExpireDuration);
}

class FExternalEditCommands final : public TCommands<FExternalEditCommands>
{
public:
	FExternalEditCommands()
		: TCommands<FExternalEditCommands>(
			"ExternalEditCommands",
			LOCTEXT("CommandsName", "External Edit Text Inputs"),
			NAME_None,
			FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(
			OpenInExternalEditor,
			"Open current text input widget in external editor.",
			"Open current text input widget in external editor.",
			EUserInterfaceActionType::Button,
			FInputChord(EKeys::E, true, true, true, false));
	}

	TSharedPtr<FUICommandInfo> OpenInExternalEditor;
};

void StartExternalEditor(
	const FString& Binary,
	const FString& ParamsFormat,
	const FString& FilePath,
	TFunction<void(int32, bool)> OnCompleted)
{
	const FString Params = FString::Format(*ParamsFormat, {FilePath});

	const bool bLaunchHidden = false;
	FInteractiveProcess CredentialProcess(*Binary, *Params, bLaunchHidden);

	bool bProcessCompleted = false;
	CredentialProcess.OnCompleted().BindLambda([&bProcessCompleted, &OnCompleted](int32 Code, bool bCanceling) {
		bProcessCompleted = true;
		// Slate functions have to be called from main thread
		Async(EAsyncExecution::TaskGraphMainThread, [OnCompleted, Code, bCanceling]() {
			OnCompleted(Code, bCanceling);
		});
	});

	if (!CredentialProcess.Launch())
	{
		Async(EAsyncExecution::TaskGraphMainThread, []() {
			ShowNotification(
				LOCTEXT("FailedToStart", "Failed to start external editor process"), SNotificationItem::CS_Fail);
		});
		return;
	}

	while (!bProcessCompleted)
	{
		FPlatformProcess::Sleep(0.001f);
	}
}

TOptional<FString> CreateExternalEditFile(const FString& CurrentContents)
{
	const FString FilePath =
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExternalTextEdit"), FGuid::NewGuid().ToString());

	if (!FFileHelper::SaveStringToFile(CurrentContents, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		ShowNotification(
			LOCTEXT("FailedToCreateFile", "Failed to create temporary edit file."), SNotificationItem::CS_Fail);
		return {};
	}

	return FilePath;
}

TOptional<FString> ReadExternalEditFile(const FString& FilePath)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *FilePath))
	{
		return {};
	}

	Contents.TrimStartAndEndInline();

	return Contents;
}

template <typename WidgetType = SMultiLineEditableText>
void OpenExternalEditorForWidget(const TSharedPtr<SWidget> Widget)
{
	WidgetType* TextWidget = static_cast<WidgetType*>(Widget.Get());

	const FString Contents = TextWidget->GetText().ToString();

	const TOptional<FString> FilePath = CreateExternalEditFile(Contents);

	if (!FilePath)
	{
		return;
	}

	auto OnCompleted = [Widget, TextWidget, FilePath](int32, bool) {
		if (const auto Contents = ReadExternalEditFile(*FilePath))
		{
			TextWidget->SetText(FText::FromString(*Contents));
			IFileManager::Get().Delete(**FilePath);
		}
	};

	auto* Settings = GetDefault<UExternalEditTextInputsSettings>();

	const FString DefaultParams = TEXT(R"("{0}")");
	const FString& Params = Settings->ParametersFormat.IsEmpty() ? DefaultParams : Settings->ParametersFormat;

	StartExternalEditor(Settings->ExternalTextEditorExecutablePath, Params, *FilePath, OnCompleted);
}

void OpenInExternalEditor()
{
	auto* Settings = GetDefault<UExternalEditTextInputsSettings>();
	if (Settings->ExternalTextEditorExecutablePath.IsEmpty())
	{
		ShowNotification(
			LOCTEXT(
				"NoExe",
				"You need to specify external text editor executable first.\n"
				"Go to the plugin settings."),
			false);
		return;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWidget> UserFocusedWidget = Slate.GetUserFocusedWidget(0);

	if (!UserFocusedWidget.IsValid())
	{
		ShowNotification(LOCTEXT("NoWidget", "No widget selected"), false);
		return;
	}

	const FName Type = UserFocusedWidget->GetType();

	if (Type == "SMultiLineEditableTextBox")
	{
		OpenExternalEditorForWidget<SMultiLineEditableTextBox>(UserFocusedWidget);
	}
	else if (Type == "SMultiLineEditableText")
	{
		OpenExternalEditorForWidget<SMultiLineEditableText>(UserFocusedWidget);
	}
	else if (Type == "SEditableText")
	{
		OpenExternalEditorForWidget<SEditableText>(UserFocusedWidget);
	}
}

void FExternalEditTextInputsModule::StartupModule()
{
	FModuleManager::GetModuleChecked<ISettingsModule>("Settings")
		.RegisterSettings(
			"Editor",
			"Plugins",
			"ExternalEditTextInputs",
			LOCTEXT("SettingsName", "External Edit Text Inputs"),
			LOCTEXT("SettingsDescription", "Configure the plugin"),
			GetMutableDefault<UExternalEditTextInputsSettings>());

	FExternalEditCommands::Register();

	TSharedRef<FUICommandList> const CmdList =
		FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").GetMainFrameCommandBindings();

	const FExternalEditCommands& Commands = FExternalEditCommands::Get();

	FUIAction const Action(FExecuteAction::CreateStatic(OpenInExternalEditor));
	CmdList->MapAction(Commands.OpenInExternalEditor, Action);
}

void FExternalEditTextInputsModule::ShutdownModule()
{
	FModuleManager::GetModuleChecked<ISettingsModule>("Settings")
		.UnregisterSettings("Editor", "Plugins", "ExternalEditTextInputs");

	TSharedRef<FUICommandList> const CmdList =
		FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").GetMainFrameCommandBindings();

	const FExternalEditCommands& Commands = FExternalEditCommands::Get();

	CmdList->UnmapAction(Commands.OpenInExternalEditor);
}

#undef LOCTEXT_NAMESPACE  //  "FExternalEditTextInputsModule"