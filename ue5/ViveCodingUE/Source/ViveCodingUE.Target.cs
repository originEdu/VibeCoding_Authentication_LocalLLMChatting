// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ViveCodingUETarget : TargetRules
{
	public ViveCodingUETarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// 에디터 타겟과 동일하게 설치 엔진(5.8)의 V7 기본값에 맞춘다.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

		ExtraModuleNames.AddRange( new string[] { "ViveCodingUE", "AuthClient" } );
	}
}
