/*
 * Author: Ronan Richardson
 * Contributors: N/A
 * Date: 18/08/2022
 * Folder: Source\Planet_NineMP\Public\
 */

#pragma once
#include "CoreMinimal.h"
#include "../Planet_NineMPCharacter.h"
#include "GameFramework/Actor.h"
#include "LightDetectionManager.generated.h"

// Forward Declarations
class UPointLightComponent;
class USpotLightComponent;
class URectLightComponent;
class UDirectionalLightComponent;

struct RectLightWrapper
{
	// Reference to the rect light this wrapper represents
	URectLightComponent* RectLight;

	// Index starts at the near plane top left, moves counterclockwise
	TArray<FVector> FrustumPoints;
	
	// Index starts at the top plane, moves counterclockwise
	TArray<FPlane> BoundingPlanes;

	RectLightWrapper(URectLightComponent* rectLight)
	{
		RectLight = rectLight;
		FrustumPoints.Init(FVector(0), 8);
		BoundingPlanes.Init(FPlane::FPlane(), 4);
	}
};

UCLASS()
class PLANET_NINEMP_API ALightDetectionManager : public AActor
{

	GENERATED_BODY()
	
public:	
	
	// Sets default values for this actor's properties
	ALightDetectionManager();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	// Called every (tick amount)
	virtual void UpdateDetection();

	void CheckPointLights(FVector PlayerPosition);
	void CheckSpotLights(FVector PlayerPosition);
	void CheckRectLights();
	void CheckDirectionalLight();

	void CalculateFrustumPoints(RectLightWrapper* rectLightWrapper);
	void CalculateBoundingPlanes(RectLightWrapper* rectLightWrapper);

	// Reference to the main character
	APlanet_NineMPCharacter* Player;

	// Dyanamic lists of all tagged lights in the scene
	TArray<UPointLightComponent*> PointLights;
	TArray<USpotLightComponent*> SpotLights;
	TArray<RectLightWrapper*> RectLights;
	UDirectionalLightComponent* MainDirectionalLight;

	// The current total light intensity that is falling on the player, unitless
	UPROPERTY(BlueprintReadWrite, Category = "Light Detection");
	float IlluminanceTotal;
	
	// The amount of light detection calculations the detection manager will perform per-second
	UPROPERTY(EditAnywhere, Category = "Light Detection");
	float UpdateFrequency = 50.0f;
	float UpdateTimer;

	// Debug command bools
	UPROPERTY(EditAnywhere, Category = "Debug");
	bool DebugIlluminanceTotal = false;
	UPROPERTY(EditAnywhere, Category = "Debug");
	bool DebugPointLights = false;
	UPROPERTY(EditAnywhere, Category = "Debug");
	bool DebugSpotLights = false;
	UPROPERTY(EditAnywhere, Category = "Debug");
	bool DebugRectLights = false;
	UPROPERTY(EditAnywhere, Category = "Debug");
	bool DebugDirectionalLight = false;

	// Undetermined
	UPROPERTY(EditAnywhere, Category = "Light Detection");
	float ForgivenessBuffer;
};
