// Fill out your copyright notice in the Description page of Project Settings.

#include "TDCore.h"
#include "Components/StaticMeshComponent.h"

ATDCore::ATDCore()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	SetRootComponent(CoreMesh);
}
