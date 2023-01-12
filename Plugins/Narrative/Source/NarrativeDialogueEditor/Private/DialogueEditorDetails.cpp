// Copyright Narrative Tools 2022. 

#include "DialogueEditorDetails.h"
#include "DetailLayoutBuilder.h"
#include "Dialogue.h"
#include "DialogueBlueprint.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "DialogueSM.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "DialogueEditorDetails"

TSharedRef<IDetailCustomization> FDialogueEditorDetails::MakeInstance()
{
	return MakeShareable(new FDialogueEditorDetails);
}


FText FDialogueEditorDetails::GetSpeakerText() const
{
	//When argument changes auto-update the description
	if (LayoutBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

		if (EditedObjects.IsValidIndex(0))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
			{
				return FText::FromName(NPCNode->SpeakerID);
			}
		}
	}

	return LOCTEXT("SpeakerText", "None");
}

TSharedRef<SWidget> FDialogueEditorDetails::MakeWidgetForOption(TSharedPtr<FText> InOption)
{
	return SNew(STextBlock).Text(*InOption);
}

void FDialogueEditorDetails::OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	//When argument changes auto-update the description
	if (LayoutBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

		if (EditedObjects.IsValidIndex(0))
		{
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
			{
				NPCNode->SpeakerID = FName(NewSelection->ToString());
			}
		}
	}
}

void FDialogueEditorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	LayoutBuilder = &DetailLayout;

	TArray<TWeakObjectPtr<UObject>> EditedObjects;
	DetailLayout.GetObjectsBeingCustomized(EditedObjects);

	if (EditedObjects.Num() > 0 && EditedObjects.IsValidIndex(0))
	{
		if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(EditedObjects[0].Get()))
		{
			if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(NPCNode->OwningDialogue->GetOuter()))
			{
				if (UDialogue* DialogueCDO = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject()))
				{
					IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Details");

					FText GroupLabel(LOCTEXT("DetailsGroup", "Details"));

					for (const auto& Speaker : DialogueCDO->Speakers)
					{
						SpeakersList.Add(MakeShareable(new FText(FText::FromName(Speaker.SpeakerID))));

						if (Speaker.SpeakerID == NPCNode->SpeakerID)
						{
							SelectedItem = SpeakersList.Last();
						}
					}

					FText RowText = LOCTEXT("SpeakerIDLabel", "Speaker");

					//Add a button to make the quest designer more simplified 
					FDetailWidgetRow& Row = Category.AddCustomRow(GroupLabel)
						.NameContent()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.Text(RowText)
						]
					.ValueContent()
						[
							SNew(SComboBox<TSharedPtr<FText>>)
							.OptionsSource(&SpeakersList)
						.OnSelectionChanged(this, &FDialogueEditorDetails::OnSelectionChanged)
						.InitiallySelectedItem(SelectedItem)
						.OnGenerateWidget_Lambda([](TSharedPtr<FText> Option)
							{
								return SNew(STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
									.Text(*Option);
							})
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.Text(this, &FDialogueEditorDetails::GetSpeakerText)
						]
						];
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE