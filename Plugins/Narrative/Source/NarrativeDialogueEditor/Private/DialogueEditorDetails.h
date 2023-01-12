// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * 
 */
class FDialogueEditorDetails : public IDetailCustomization
{

public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/**We cache a reference to this so we can do refreshes*/
	IDetailLayoutBuilder* LayoutBuilder;

	TArray<TSharedPtr<FText>> SpeakersList;
	TSharedPtr<FText> SelectedItem;

	FText GetSpeakerText() const;
	TSharedRef<SWidget> MakeWidgetForOption(TSharedPtr<FText> InOption);

	virtual void OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

};
