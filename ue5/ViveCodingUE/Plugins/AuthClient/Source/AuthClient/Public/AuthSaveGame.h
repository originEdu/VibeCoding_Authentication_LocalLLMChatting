#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AuthSaveGame.generated.h"

/** refresh 토큰을 디스크에 영속화하기 위한 SaveGame. */
UCLASS()
class AUTHCLIENT_API UAuthSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString RefreshToken;
};
