#pragma once

#include "CoreMinimal.h"

class FExternalEditTextInputsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
