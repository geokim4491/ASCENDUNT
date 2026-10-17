// Fill out your copyright notice in the Description page of Project Settings.


#include "AscMainMenuPageWidgetBase.h"
#include "AscMainMenuWidget.h"

void UAscMainMenuPageWidgetBase::InitializePage(UAscMainMenuWidget* InOwnerMainMenu)
{
    OwnerMainMenu = InOwnerMainMenu;
}

void UAscMainMenuPageWidgetBase::OnPageActivated()
{
}

void UAscMainMenuPageWidgetBase::OnPageDeactivated()
{
}
