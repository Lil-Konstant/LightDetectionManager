// Fill out your copyright notice in the Description page of Project Settings.

#include "LightDetectionManager.h"
#include <cmath>
#include "EngineUtils.h"
#include "Containers/Array.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Math/Plane.h"

// Sets default values
ALightDetectionManager::ALightDetectionManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

/// <summary>
/// BeginPlay() first calls the base class BeginPlay(), and then will store a reference to the player character using the UGameplayStatistics class.
/// The function then iterates through all active objects in the scene and stores actors tagged with Spot Light, Point Light or Rect Light into their
/// respective TArrays, and then finally initialises the UpdateTimer as the inverse of whatever the UpdateFrequency has been set to in editor.
/// </summary>
void ALightDetectionManager::BeginPlay()
{
	Super::BeginPlay();
	
	// Store a reference to the player character by attempting to cast it from the base ACharacter class into its player character child class
	Player = dynamic_cast<APlanet_NineMPCharacter*>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Iterate through all actors in the scene, checking for point, spot, and rect light tags
	for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;

		// If the actor is tagged as a point, spot, or rect light, add a reference of it to it's respective array
		if (Actor->ActorHasTag(TEXT("Point Light")))
		{
			UPointLightComponent* PointLightComponent = Actor->FindComponentByClass<UPointLightComponent>();
			if (PointLightComponent)
			{
				PointLights.Add(PointLightComponent);
				continue;
			}
		}
		else if (Actor->ActorHasTag(TEXT("Spot Light")))
		{
			USpotLightComponent* SpotLightComponent = Actor->FindComponentByClass<USpotLightComponent>();
			if (SpotLightComponent)
			{
				SpotLights.Add(SpotLightComponent);
				continue;
			}
		}
	}

	// Set the update timer based on the update frequency that has been set in editor
	UpdateTimer = 1 / UpdateFrequency;
}

/// <summary>
/// UpdateDetection() iterates through all lights in each of the light arrays, and if they are within range of their
/// attenuation radius, calculates their relative lighting contribution to the player to calculate a CurrentLightTotal, which
/// is the total amount of light intensity that is currently falling on the player. The BP_LightMeter class uses the CurrentLightTotal
/// to update the LightTotalPercentage and display it on the LightMeterUI.
/// </summary>
void ALightDetectionManager::UpdateDetection()
{	
	// Illuminance total on the player for this update tick
	IlluminanceTotal = 0.0f;

	FVector PlayerPosition = Player->GetActorLocation();
	FVector DetectionPoint;
	FHitResult HitResult;
	// If there is a floor below the player, check if it is within standing range
	if (GetWorld()->LineTraceSingleByChannel(HitResult, PlayerPosition, PlayerPosition + (100 * FVector::DownVector), ECollisionChannel::ECC_GameTraceChannel5))
	{
		// If the player is standing on the detected floor below them, use it as the detection point for light detection
		if (FVector::Distance(HitResult.Location, PlayerPosition) < 98)
		{
			FString dist = FString::SanitizeFloat(FVector::Distance(HitResult.Location, PlayerPosition));
			if (GEngine) GEngine->AddOnScreenDebugMessage(4, 0.1f, FColor::Red, FString::Printf(TEXT("floor distance: %s"), *dist));
			
			DetectionPoint = HitResult.Location + (10 * FVector::UpVector);
		}
	}
	// Otherwise just use the player's approximate feet position for the detection point if not on the floor
	else
	{
		DetectionPoint = PlayerPosition + (93.980003 * FVector::DownVector);
		if (GEngine) GEngine->AddOnScreenDebugMessage(5, 0.1f, FColor::Red, FString::Printf(TEXT("no hit floor")));
	}

	CheckPointLights(DetectionPoint);
	CheckSpotLights(DetectionPoint);
	
	//CheckRectLights();
	//CheckDirectionalLight();

	// Print the current light total to the screen
	if (DebugIlluminanceTotal)
	{
		FString LightTotalString = FString::SanitizeFloat(IlluminanceTotal);
		if (GEngine) GEngine->AddOnScreenDebugMessage(1, 0.1f, FColor::Red, FString::Printf(TEXT("Current Intensity Total: %s"), *LightTotalString));
	}
}

void ALightDetectionManager::CheckPointLights(FVector PlayerPosition)
{
	// Placeholder variable for the line trace results
	FHitResult HitResult;

	// For each point light in the point lights array
	for (int idx = 0; idx < PointLights.Num(); idx++)
	{
		// If this point light is not visible in the scene, skip it
		if (!PointLights[idx]->IsVisible() || PointLights[idx]->Intensity <= 0)
		{
			return;
		}
		
		// Cache  the light and player positions for use
		FVector4 LightPosition = PointLights[idx]->GetLightPosition();

		// Draw a debug line from this point light to the player
		if (DebugPointLights)
		{
			DrawDebugLine(GetWorld(), LightPosition, PlayerPosition, FColor::Green, false, 0.15f, 0, 0.5f);
		}

		// Store the distance from light to player, if it exceeds this light's attenuation radius plus a buffer amount, skip this light's contribution
		float LightDistanceSqr = FVector::DistSquared(LightPosition, PlayerPosition);
		if (LightDistanceSqr > (PointLights[idx]->AttenuationRadius * PointLights[idx]->AttenuationRadius) + ForgivenessBuffer)
		{
			continue;
		}

		// If there is nothing between this light and the player, set InLight to true and add this lights relative intensity to the temporary total
		
		{
			IlluminanceTotal = 1.0f;

			//////////////////////////////////////////// OLD PHOTOMETRY MATHS ////////////////////////////////////////////
			//float LightDistance = FMath::Sqrt(LightDistanceSqr) * 0.01f;
			//IlluminanceTotal += (PointLights[idx]->Intensity) / (4 * PI * LightDistance);
		}
	}
}

void ALightDetectionManager::CheckSpotLights(FVector PlayerPosition)
{
	// Placeholder variable for the line trace results
	FHitResult HitResult;

	// For each spot light in the spot lights array
	for (int idx = 0; idx < SpotLights.Num(); idx++)
	{
		// If this spot light light is not visible in the scene or the intensity is zero, skip it
		if (!SpotLights[idx]->IsVisible())
		{
			continue;
		}

		// Cache  the light and player positions for use, as well as the spot light forward direction
		FVector4 SpotLightPosition = SpotLights[idx]->GetLightPosition();
		FVector SpotLightDir = SpotLights[idx]->GetForwardVector();
		FVector PlayerDisplacement = PlayerPosition - SpotLightPosition;
		
		// Draw a debug line from this point light to the player
		if (DebugSpotLights)
		{
			DrawDebugLine(GetWorld(), SpotLightPosition, PlayerPosition, FColor::Green, false, 0.15f, 0, 0.5f);
		}

		// If the player is not in range of the spotlight's cone height, do not include this spot light in the CurrentLightTotal calculation
		float LightDistanceSqr = FVector::DistSquared(SpotLightPosition, PlayerPosition);
		float AngleBetween = FMath::Acos(FVector::DotProduct(PlayerDisplacement, SpotLightDir) / PlayerDisplacement.Size());
		float ConeHeight = SpotLights[idx]->AttenuationRadius * (FMath::Cos(SpotLights[idx]->OuterConeAngle * (PI / 180)) / FMath::Cos(AngleBetween));
		if (LightDistanceSqr > (ConeHeight * ConeHeight) + ForgivenessBuffer)
		{
			continue;
		}

		// If the player is in range but not within "view" of the spot light, do not include this spot light in the CurrentLightTotal calculation
		float SpotLightToPlayerAngle = FMath::Acos(FVector::DotProduct(SpotLightDir, PlayerDisplacement.GetSafeNormal())) * (180 / PI);
		if (SpotLightToPlayerAngle > SpotLights[idx]->OuterConeAngle)
		{
			continue;
		}

		// If there is nothing between this light and the player, set InLight to true and add this lights relative intensity to the temporary total
		if (!GetWorld()->LineTraceSingleByChannel(HitResult, SpotLightPosition, PlayerPosition, ECollisionChannel::ECC_GameTraceChannel5))
		{
			if (SpotLights[idx]->Intensity <= 0)
			{
				continue;
			}

			//if (GEngine && DebugSpotLights) GEngine->AddOnScreenDebugMessage(4, 0.1f, FColor::Red, SpotLights[idx]->GetOwner()->GetName());
			IlluminanceTotal = 1.0f;

			//////////////////////////////////////////// OLD PHOTOMETRY MATHS ////////////////////////////////////////////
			//// Linearly scale the luminous power down if the player is between the inner and outer cones, otherwise leave it as the full intensity
			//float LuminousPower = LuminousPower = SpotLights[idx]->Intensity;
			//if (SpotLightToPlayerAngle > SpotLights[idx]->InnerConeAngle)
			//{
			//	LuminousPower *= (1 - ((SpotLightToPlayerAngle - SpotLights[idx]->InnerConeAngle) / (SpotLights[idx]->OuterConeAngle - SpotLights[idx]->InnerConeAngle)));
			//}

			//// Find the surface area of the spherical sector of the spot light at the player's distance
			//float LightDistance = FMath::Sqrt(LightDistanceSqr) * 0.01f;
			//float SpotLightSurfaceArea = 2 * PI * (1 - cos(SpotLights[idx]->OuterConeAngle * (PI / 180))) * LightDistance;
			//IlluminanceTotal += LuminousPower / SpotLightSurfaceArea;
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(3, 5.0f, FColor::Red, HitResult.GetActor()->GetName());
		}
	}
}

void ALightDetectionManager::CheckRectLights()
{
	// Placeholder variable for the line trace results
	FHitResult HitResult;
	FVector PlayerPosition = Player->GetActorLocation();

	// For each rect light in the rect lights wrapper array
	for (int idx = 0; idx < RectLights.Num(); idx++)
	{
		// If this rect light is not visible in the scene, skip it
		if (!RectLights[idx]->RectLight->IsVisible())
		{
			return;
		}

		FVector LightPosition = RectLights[idx]->RectLight->GetLightPosition();

		// Store the distance from light to player, if it exceeds this light's attenuation radius plus a buffer amount, skip this light's contribution
		float LightDistanceSqr = FVector::DistSquared(LightPosition, PlayerPosition);
		if (LightDistanceSqr > (RectLights[idx]->RectLight->AttenuationRadius * RectLights[idx]->RectLight->AttenuationRadius) + ForgivenessBuffer)
		{
			continue;
		}

		if (!GetWorld()->LineTraceSingleByChannel(HitResult, LightPosition, PlayerPosition, ECollisionChannel::ECC_GameTraceChannel5))
		{
			// If this rect light is dynamic, re-calculate the frustum points and bounding planes
			if (true)
			{
				CalculateFrustumPoints(RectLights[idx]);
				CalculateBoundingPlanes(RectLights[idx]);
			}

			// Check if the player is above all 4 bounding planes
			float TopPlaneDist = FPlane::PointPlaneDist(PlayerPosition, RectLights[idx]->FrustumPoints[3], RectLights[idx]->BoundingPlanes[0].GetNormal());
			float RightPlaneDist = FPlane::PointPlaneDist(PlayerPosition, RectLights[idx]->FrustumPoints[0], RectLights[idx]->BoundingPlanes[1].GetNormal());
			float BottomPlaneDist = FPlane::PointPlaneDist(PlayerPosition, RectLights[idx]->FrustumPoints[0], RectLights[idx]->BoundingPlanes[2].GetNormal());
			float LeftPlaneDist = FPlane::PointPlaneDist(PlayerPosition, RectLights[idx]->FrustumPoints[1], RectLights[idx]->BoundingPlanes[3].GetNormal());
			// If the player is infront of all the bounding planes, calculate the relative illuminance from this light as if it's a point light
			if (TopPlaneDist > 0 && RightPlaneDist > 0 && BottomPlaneDist > 0 && LeftPlaneDist > 0)
			{
				float LightDistance = FMath::Sqrt(LightDistanceSqr) * 0.01f;
				IlluminanceTotal += (RectLights[idx]->RectLight->Intensity) / (2 * PI * LightDistance);
			}
		}

		/////// DEBUG DRAWING ///////
		if (DebugRectLights)
		{
			// Draw each of the points for this rect light frustum
			for (int pointIdx = 0; pointIdx < RectLights[idx]->FrustumPoints.Num(); pointIdx++)
			{
				DrawDebugPoint(GetWorld(), RectLights[idx]->FrustumPoints[pointIdx], 10.0f, FColor::Red);
			}

			// Draw the four bounding planes, counterclockwise starting from the top plane
			DrawDebugSolidPlane(GetWorld(), RectLights[idx]->BoundingPlanes[0], (RectLights[idx]->FrustumPoints[2] + RectLights[idx]->FrustumPoints[3]) / 2, FVector2D(200, 500), FColor::Purple, false, 0.05f);
			DrawDebugSolidPlane(GetWorld(), RectLights[idx]->BoundingPlanes[1], (RectLights[idx]->FrustumPoints[0] + RectLights[idx]->FrustumPoints[3]) / 2, FVector2D(700, 500), FColor::Yellow, false, 0.05f);
			DrawDebugSolidPlane(GetWorld(), RectLights[idx]->BoundingPlanes[2], (RectLights[idx]->FrustumPoints[0] + RectLights[idx]->FrustumPoints[1]) / 2, FVector2D(200, 500), FColor::Orange, false, 0.05f);
			DrawDebugSolidPlane(GetWorld(), RectLights[idx]->BoundingPlanes[3], (RectLights[idx]->FrustumPoints[1] + RectLights[idx]->FrustumPoints[2]) / 2, FVector2D(700, 500), FColor::Red, false, 0.05f);

			// Draw a debug line from this point light to the player (DEBUG ONLY)
			DrawDebugLine(GetWorld(), LightPosition, PlayerPosition, FColor::Green, false, 0.015f, 0, 0.5f);
		}
	}
}

void ALightDetectionManager::CheckDirectionalLight()
{
	// If there is not directional light in the scene, skip it
	if (!MainDirectionalLight)
	{
		return;
	}

	// If the main directional light is not visible, skip it
	if (!MainDirectionalLight->IsVisible())
	{
		return;
	}

	// Placeholder variable for the line trace results
	FHitResult HitResult;

	// Cache  the light and player positions for use, as well as the spot light forward direction
	FVector LightDirection = MainDirectionalLight->GetForwardVector();
	FVector PlayerPosition = Player->GetActorLocation();
	// Get a position of the directional light, 5000cm from the player along the directional light's forward vector
	FVector DirecitonalLightPosition = PlayerPosition - (LightDirection * 5000);

	if (!GetWorld()->LineTraceSingleByChannel(HitResult, DirecitonalLightPosition, PlayerPosition, ECollisionChannel::ECC_Visibility))
	{
		IlluminanceTotal += MainDirectionalLight->Intensity;
	}

	// Draw a debug line from this point light to the player (DEBUG ONLY)
	if (DebugDirectionalLight)
	{
		DrawDebugLine(GetWorld(), DirecitonalLightPosition, PlayerPosition, FColor::Green, false, 0.015f, 0, 0.5f);
	}
}

void ALightDetectionManager::CalculateFrustumPoints(RectLightWrapper* rectLightWrapper)
{
	// Top left, near plane
	rectLightWrapper->FrustumPoints[0] = rectLightWrapper->RectLight->GetLightPosition() - (rectLightWrapper->RectLight->GetRightVector() * (rectLightWrapper->RectLight->SourceWidth / 2)) + (rectLightWrapper->RectLight->GetUpVector() * (rectLightWrapper->RectLight->SourceHeight / 2));

	// Top right, near plane
	rectLightWrapper->FrustumPoints[1] = rectLightWrapper->RectLight->GetLightPosition() + (rectLightWrapper->RectLight->GetRightVector() * (rectLightWrapper->RectLight->SourceWidth / 2)) + (rectLightWrapper->RectLight->GetUpVector() * (rectLightWrapper->RectLight->SourceHeight / 2));;

	// Bottom right, near plane
	rectLightWrapper->FrustumPoints[2] = rectLightWrapper->RectLight->GetLightPosition() + (rectLightWrapper->RectLight->GetRightVector() * (rectLightWrapper->RectLight->SourceWidth / 2)) - (rectLightWrapper->RectLight->GetUpVector() * (rectLightWrapper->RectLight->SourceHeight / 2));;

	// Bottom left, near plane
	rectLightWrapper->FrustumPoints[3] = rectLightWrapper->RectLight->GetLightPosition() - (rectLightWrapper->RectLight->GetRightVector() * (rectLightWrapper->RectLight->SourceWidth / 2)) - (rectLightWrapper->RectLight->GetUpVector() * (rectLightWrapper->RectLight->SourceHeight / 2));;

	// Top left, far plane
	FVector farPlaneSegment = rectLightWrapper->FrustumPoints[0] + (rectLightWrapper->RectLight->GetForwardVector() * rectLightWrapper->RectLight->BarnDoorLength).RotateAngleAxis(-rectLightWrapper->RectLight->BarnDoorAngle, rectLightWrapper->RectLight->GetRightVector());
	float farPlaneSegmentLength = rectLightWrapper->RectLight->BarnDoorLength * sinf(rectLightWrapper->RectLight->BarnDoorAngle * (PI / 180));
	rectLightWrapper->FrustumPoints[4] = farPlaneSegment - (rectLightWrapper->RectLight->GetRightVector() * farPlaneSegmentLength);

	// Top right, far plane
	rectLightWrapper->FrustumPoints[5] = rectLightWrapper->FrustumPoints[4] + (rectLightWrapper->RectLight->GetRightVector() * (2 * farPlaneSegmentLength + rectLightWrapper->RectLight->SourceWidth));

	// Bottom right, far plane
	rectLightWrapper->FrustumPoints[6] = rectLightWrapper->FrustumPoints[5] - (rectLightWrapper->RectLight->GetUpVector() * (2 * farPlaneSegmentLength + rectLightWrapper->RectLight->SourceHeight));

	// Bottom left, far plane
	rectLightWrapper->FrustumPoints[7] = rectLightWrapper->FrustumPoints[6] - (rectLightWrapper->RectLight->GetRightVector() * (2 * farPlaneSegmentLength + rectLightWrapper->RectLight->SourceWidth));
}

void ALightDetectionManager::CalculateBoundingPlanes(RectLightWrapper* rectLightWrapper)
{
	// Calculate the top bounding plane
	FVector topPlaneNormal = FVector::CrossProduct(rectLightWrapper->FrustumPoints[3] - rectLightWrapper->FrustumPoints[2], rectLightWrapper->FrustumPoints[4] - rectLightWrapper->FrustumPoints[2]);
	topPlaneNormal.Normalize();
	rectLightWrapper->BoundingPlanes[0] = FPlane(topPlaneNormal, FVector::DotProduct(topPlaneNormal, rectLightWrapper->FrustumPoints[2])).Flip();

	// Calculate the right bounding plane
	FVector rightPlaneNormal = FVector::CrossProduct(rectLightWrapper->FrustumPoints[0] - rectLightWrapper->FrustumPoints[3], rectLightWrapper->FrustumPoints[5] - rectLightWrapper->FrustumPoints[3]);
	rightPlaneNormal.Normalize();
	rectLightWrapper->BoundingPlanes[1] = FPlane(rightPlaneNormal, FVector::DotProduct(rightPlaneNormal, rectLightWrapper->FrustumPoints[0])).Flip();

	// Calculate the bottom bounding plane
	FVector bottomPlaneNormal = FVector::CrossProduct(rectLightWrapper->FrustumPoints[7] - rectLightWrapper->FrustumPoints[1], rectLightWrapper->FrustumPoints[0] - rectLightWrapper->FrustumPoints[1]);
	bottomPlaneNormal.Normalize();
	rectLightWrapper->BoundingPlanes[2] = FPlane(bottomPlaneNormal, FVector::DotProduct(bottomPlaneNormal, rectLightWrapper->FrustumPoints[0])).Flip();

	// Calculate the left bounding plane
	FVector leftPlaneNormal = FVector::CrossProduct(rectLightWrapper->FrustumPoints[4] - rectLightWrapper->FrustumPoints[2], rectLightWrapper->FrustumPoints[1] - rectLightWrapper->FrustumPoints[2]);
	leftPlaneNormal.Normalize();
	rectLightWrapper->BoundingPlanes[3] = FPlane(leftPlaneNormal, FVector::DotProduct(leftPlaneNormal, rectLightWrapper->FrustumPoints[1])).Flip();
}

/// <summary>
/// Every Tick, the function will decrement the UpdateTimer by the amount of time passed, and if the timer has reached zero,
/// a call to UpdateDetection() is made and the UpdateTimer is reset. The function then checks to see if the player is currently
/// not in light and if the CurrentLightTotal is still above zero, and will decrement it towards zero at a rate determined by the
/// DrainSpeed.
/// </summary>
void ALightDetectionManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateTimer -= DeltaTime;
	// If the updateTimer has run out, update the light detection and reset the timer
	if (UpdateTimer <= 0) 
	{ 
		// Call a detection update
		UpdateDetection(); 
		// Reset the update timer
		UpdateTimer = 1 / UpdateFrequency; 
	}
}

