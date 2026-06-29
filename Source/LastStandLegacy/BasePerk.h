// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BasePerk.generated.h"

UCLASS(Abstract, Blueprintable)
class LASTSTANDLEGACY_API ABasePerk : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABasePerk();

protected:
    virtual void BeginPlay() override;

    // --- بەشی پێکهاتە فیزیکییەکان (Components) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* PerkMachineMesh;

    // --- داتای پێرک (Perk Configuration) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk Settings")
    FName PerkID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk Settings")
    int32 PerkCost;

    // مێشی ئەو بوتڵەی کە کارەکتەر دەیخواتەوە (لە ناو بلۆپرێنتی چایڵد دیاری دەکرێت)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk Settings")
    UStaticMesh* BottleMesh;

public:
    // جێبەجێکردنی فەنکشنی ئینتەرفەیس بۆ کارلێککردن
    virtual void Interact(class AHama* HamaChar) override;

    // گێتەرەکان بۆ ئەوەی کارەکتەر بتوانێت داتاکانی ئەم پێرکە بخوێنێتەوە (قۆناغی AAA)
    FName GetPerkID() const { return PerkID; }
    int32 GetPerkCost() const { return PerkCost; }
    UStaticMesh* GetBottleMesh() const { return BottleMesh; }
};