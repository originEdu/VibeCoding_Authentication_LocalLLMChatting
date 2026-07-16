// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ViveCodingUEEditorTarget : TargetRules
{
	public ViveCodingUEEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// 설치된 UE 5.8 UnrealEditor 는 V7 기본값(세 경고 레벨=Error)으로 빌드됨.
		// 산출물을 공유하는 에디터 타겟은 이를 일치시켜야 하므로 V7 사용.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		// include 순서 변경으로 인한 컴파일 변동을 막기 위해 현재 순서를 명시 고정.
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

		ExtraModuleNames.AddRange( new string[] { "ViveCodingUE", "AuthClient" } );
	}
}
