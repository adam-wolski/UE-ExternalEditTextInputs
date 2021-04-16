#pragma once

#include "CoreMinimal.h"

#include "ExternalEditTextInputsSettings.generated.h"

UCLASS(config = Plugins, DefaultConfig)
class UExternalEditTextInputsSettings final : public UObject
{
	GENERATED_BODY()
public:
	/** Path to the external text editor executable */
	UPROPERTY(Config, EditAnywhere, Category = Settings)
	FString ExternalTextEditorExecutablePath;

	/**
	 * Parameters format to be used when passing file parameter to external editor.
	 * Most of editors accept file path as last parameter and nothing needs to be changed and this option can be left empty.
	 * 
	 * Some editors need specific parameter format like '--filepath=<TemporaryEdit/Filepath/Here.txt>'
	 * For this example you would need to specify this property as '--filepath="{0}"'
	 * The {0} will be expended to full Filepath to temporary file that is then read into text input widget.
	 *
	 * This can be also used to specify additional parameters to your external editor like:
	 * '--quiet --open-fast --no-plugins "{0}"'
	 *
	 * Which then will be executed as:
	 * 'ExternalTextEditorExecutablePath --quiet --open-fast --no-plugins "/Path/To/Temporary/EditFile.txt"'
	 */
	UPROPERTY(Config, EditAnywhere, Category = Settings)
	FString ParametersFormat;
};
