// Copyright Narrative Tools 2022. 

#include "DialogueGraphEditor.h"
#include "NarrativeDialogueEditorModule.h"
#include "ScopedTransaction.h"
#include "DialogueEditorTabFactories.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "DialogueGraphNode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EdGraphUtilities.h"
#include "DialogueGraph.h"
#include "DialogueGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DialogueEditorModes.h"
#include "Widgets/Docking/SDockTab.h"
#include "DialogueEditorToolbar.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "DialogueSM.h"
#include "DialogueAsset.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphNode.h"
#include "Dialogue.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h" 
#include "DialogueEditorCommands.h"
#include "DialogueEditorSettings.h"
#include <SBlueprintEditorToolbar.h>
#include <Kismet2/DebuggerCommands.h>
#include "SKismetInspector.h"

#define LOCTEXT_NAMESPACE "DialogueAssetEditor"

const FName FDialogueGraphEditor::DialogueEditorMode(TEXT("DialogueEditor"));

//TODO change this to the documentation page after documentation is ready for use
static const FString NarrativeHelpURL("http://www.google.com");

FDialogueGraphEditor::FDialogueGraphEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FDialogueGraphEditor::~FDialogueGraphEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo(this);
	}
}


void FDialogueGraphEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	if (NewSelection.Num() == 1)
	{
		for (auto& Obj : NewSelection)
		{
			//Want to edit the underlying dialogue object, not the graph node
			if (UDialogueGraphNode* GraphNode = Cast<UDialogueGraphNode>(Obj))
			{
				TSet<class UObject*> ModifiedSelection;
				ModifiedSelection.Add(GraphNode->DialogueNode);
				FBlueprintEditor::OnSelectedNodesChangedImpl(ModifiedSelection);
				return;
			}
		}
	}


	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);
}
//
//void FDialogueGraphEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
//{
//	SelectedNodesCount = NewSelection.Num();
//
//	if (SelectedNodesCount == 0)
//	{
//		DetailsView->SetObject(DialogueBlueprint->Dialogue);
//		return;
//	}
//
//	UDialogueGraphNode* SelectedNode = nullptr;
//
//	for (UObject* Selection : NewSelection)
//	{
//		if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(Selection))
//		{
//			SelectedNode = Node;
//			break;
//		}
//	}
//
//	if (UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph))
//	{
//		if (DetailsView.IsValid())
//		{
//			if (SelectedNode)
//			{
//				//Edit the underlying graph node object 
//				if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(SelectedNode))
//				{
//					DetailsView->SetObject(Node->DialogueNode);
//				}
//			}
//		}
//		else
//		{
//			DetailsView->SetObject(nullptr);
//		}
//	}
//}

void FDialogueGraphEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDialogueBlueprint* InDialogue)
{
	DialogueBlueprint = InDialogue; 

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	CreateDefaultCommands();
	//BindCommands();
	RegisterMenus();

	CreateInternalWidgets();

	TSharedPtr<FDialogueGraphEditor> ThisPtr(SharedThis(this));

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(DialogueBlueprint);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FNarrativeDialogueEditorModule::DialogueEditorAppId, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	TArray<UBlueprint*> EditedBlueprints;
	EditedBlueprints.Add(DialogueBlueprint);

	CommonInitialization(EditedBlueprints, false);

	AddApplicationMode(DialogueEditorMode, MakeShareable(new FDialogueEditorApplicationMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	SetCurrentMode(DialogueEditorMode);

	PostLayoutBlueprintEditorInitialization();

}

FName FDialogueGraphEditor::GetToolkitFName() const
{
	return FName("Dialogue Editor");
}

FText FDialogueGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "DialogueEditor");
}

FString FDialogueGraphEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DialogueEditor").ToString();
}

FLinearColor FDialogueGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

FText FDialogueGraphEditor::GetToolkitToolTipText() const
{
	if (DialogueBlueprint)
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(DialogueBlueprint);
	}
	return FText();
}

void FDialogueGraphEditor::CreateDialogueCommandList()
{
	if (GraphEditorCommands.IsValid())
	{
		GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_SelectAllNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanSelectAllNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_DeleteSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanDeleteNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CopySelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanCopyNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CutSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanCutNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_PasteNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanPasteNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_DuplicateNodes),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanDuplicateNodes)
		);

		GraphEditorCommands->MapAction(FGraphEditorCommands::Get().CreateComment,
			FExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CreateComment),
			FCanExecuteAction::CreateRaw(this, &FDialogueGraphEditor::Dialogue_CanCreateComment)
		);

	}
}


void FDialogueGraphEditor::PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& GraphLocation)
{

	if (UDialogueGraph* DGraph = Cast<UDialogueGraph>(DestinationGraph))
	{
		Dialogue_PasteNodesHere(GraphLocation);
	}
	else
	{
		FBlueprintEditor::PasteNodesHere(DestinationGraph, GraphLocation);
	}
}

void FDialogueGraphEditor::Dialogue_SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FDialogueGraphEditor::Dialogue_CanSelectAllNodes() const
{
	return true;
}

void FDialogueGraphEditor::Dialogue_DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	CurrentGraphEditor->GetCurrentGraph()->Modify();

	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				Node->Modify();
				Node->DestroyNode();
			}
		}
	}
}

bool FDialogueGraphEditor::Dialogue_CanDeleteNodes() const
{
	// If any of the nodes can be deleted then we should allow deleting
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}

void FDialogueGraphEditor::Dialogue_DeleteSelectedDuplicatableNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}
	const FGraphPanelSelectionSet OldSelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FDialogueGraphEditor::Dialogue_CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FDialogueGraphEditor::Dialogue_CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FDialogueGraphEditor::Dialogue_CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
		if (Node == nullptr)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

}

bool FDialogueGraphEditor::Dialogue_CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FDialogueGraphEditor::Dialogue_PasteNodes()
{

	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
	{
		Dialogue_PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FDialogueGraphEditor::Dialogue_PasteNodesHere(const FVector2D& Location)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph());

	DialogueGraph->Modify();

	UDialogueGraphNode* SelectedParent = NULL;
	bool bHasMultipleNodesSelected = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(DialogueGraph, TextToImport, /*out*/ PastedNodes);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* PasteNode = *It;
		UDialogueGraphNode* PasteDialogueNode = Cast<UDialogueGraphNode>(PasteNode);


		if (PasteNode && PasteDialogueNode)
		{
			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(PasteNode, true);

			PasteNode->NodePosX += 200.f;
			PasteNode->NodePosY += 200.f;

			PasteNode->SnapToGrid(16);

			// Give new node a different Guid from the old one
			PasteNode->CreateNewGuid();

			//New dialogue graph node will point to old dialouenode, duplicate a new one for our new node
			UDialogueNode* DupNode = Cast<UDialogueNode>(StaticDuplicateObject(PasteDialogueNode->DialogueNode, PasteDialogueNode->DialogueNode->GetOuter()));

			//StaticDuplicateObject won't have assigned a unique ID, grab a unique one
			DupNode->EnsureUniqueID();

			//StaticDuplicateObject won't have added node to dialogues array
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(DupNode))
			{
				DialogueBlueprint->DialogueTemplate->NPCReplies.Add(NPCNode);
			}
			else if (UDialogueNode_Player* PlayerNode = Cast<UDialogueNode_Player>(DupNode))
			{
				DialogueBlueprint->DialogueTemplate->PlayerReplies.Add(PlayerNode);
			}

			PasteDialogueNode->DialogueNode = DupNode;
		}
	}

	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	UObject* GraphOwner = DialogueGraph->GetOuter();
	if (GraphOwner)
	{
		GraphOwner->PostEditChange();
		GraphOwner->MarkPackageDirty();
	}
}

bool FDialogueGraphEditor::Dialogue_CanPasteNodes() const
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FDialogueGraphEditor::Dialogue_DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FDialogueGraphEditor::Dialogue_CanDuplicateNodes() const
{
	//Duplicating nodes is disabled for now
	return false;

	return CanCopyNodes();
}

void FDialogueGraphEditor::Dialogue_CreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = UpdateGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				FDialogueSchemaAction_AddComment CommentAction;
				CommentAction.PerformAction(Graph, NULL, GraphEditor->GetPasteLocation());
			}
		}
	}
}

bool FDialogueGraphEditor::Dialogue_CanCreateComment() const
{
	return true;
}


bool FDialogueGraphEditor::Dialogue_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
{
	const bool bResult = FBlueprintEditor::GetBoundsForSelectedNodes(Rect, Padding);

	return bResult;
}

void FDialogueGraphEditor::ShowDialogueDetails()
{
	DetailsView->SetObject(DialogueBlueprint);
}

bool FDialogueGraphEditor::CanShowDialogueDetails() const
{
	return IsValid(DialogueBlueprint);
}

void FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser()
{
	FPlatformProcess::LaunchURL(*NarrativeHelpURL, NULL, NULL);
}

bool FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser() const
{
	return true;
}

void FDialogueGraphEditor::QuickAddNode()
{
	//Disabled for now until we can find out why creating actions cause a nullptr crash
	return;

	//TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	//if (!CurrentGraphEditor.IsValid())
	//{
	//	return;
	//}

	//const FScopedTransaction Transaction(FDialogueEditorCommands::Get().QuickAddNode->GetDescription());
	//UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph());

	//DialogueGraph->Modify();

	//const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	//if (SelectedNodes.Num() == 0)
	//{
	//	return;
	//}

	//if (const UDialogueGraphSchema* Schema = Cast<UDialogueGraphSchema>(DialogueGraph->GetSchema()))
	//{
	//	for (auto& SelectedNode : SelectedNodes)
	//	{
	//		FDialogueSchemaAction_NewNode AddNewNode;
	//		UDialogueGraphNode* Node;
	//		//We we're quick adding from an action, add a new state after the action
	//		if (UDialogueGraphNode_Action* ActionNode = Cast<UDialogueGraphNode_Action>(SelectedNode))
	//		{
	//			//Dont do anything if we're already linked somewhere
	//			if (ActionNode->GetOutputPin()->LinkedTo.Num() == 0)
	//			{
	//				Node = NewObject<UDialogueGraphNode_State>(DialogueGraph, UDialogueGraphNode_State::StaticClass());
	//				AddNewNode.NodeTemplate = Node;

	//				FVector2D NewStateLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

	//				NewStateLocation.X += 300.f;

	//				AddNewNode.PerformAction(DialogueGraph, ActionNode->GetOutputPin(), NewStateLocation);


	//				// Update UI
	//				CurrentGraphEditor->NotifyGraphChanged();

	//				UObject* GraphOwner = DialogueGraph->GetOuter();
	//				if (GraphOwner)
	//				{
	//					GraphOwner->PostEditChange();
	//					GraphOwner->MarkPackageDirty();
	//				}
	//			}
	//		}
	//		else if (UDialogueGraphNode_State* StateNode = Cast<UDialogueGraphNode_State>(SelectedNode))
	//		{
	//			//Dont allow adding nodes from a failure or success node
	//			if (!(StateNode->IsA<UDialogueGraphNode_Failure>() || StateNode->IsA<UDialogueGraphNode_Success>()))
	//			{
	//				//For some reason this line crashes the editor and so quick add is disabled for now.
	//				Node = NewObject<UDialogueGraphNode_Action>(DialogueGraph, UDialogueGraphNode_Action::StaticClass());
	//				AddNewNode.NodeTemplate = Node;

	//				FVector2D NewActionLocation = FVector2D(ActionNode->NodePosX, ActionNode->NodePosY);

	//				NewActionLocation.X += 300.f;

	//				//Make sure new node doesn't overlay any others
	//				for (auto& LinkedTo : ActionNode->GetOutputPin()->LinkedTo)
	//				{
	//					if (LinkedTo->GetOwningNode()->NodePosY < NewActionLocation.Y)
	//					{
	//						NewActionLocation.Y = LinkedTo->GetOwningNode()->NodePosY - 200.f;
	//					}
	//				}

	//				AddNewNode.PerformAction(DialogueGraph, ActionNode->GetOutputPin(), NewActionLocation);


	//				// Update UI
	//				CurrentGraphEditor->NotifyGraphChanged();

	//				UObject* GraphOwner = DialogueGraph->GetOuter();
	//				if (GraphOwner)
	//				{
	//					GraphOwner->PostEditChange();
	//					GraphOwner->MarkPackageDirty();
	//				}
	//			}
	//		}
	//	}
	//}
}

bool FDialogueGraphEditor::CanQuickAddNode() const
{
	return true;
}

void FDialogueGraphEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FEditorUndoClient::PostUndo(bSuccess);

	if (DialogueBlueprint->DialogueGraph)
	{
		// Update UI
		DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
		if (DialogueBlueprint)
		{
			DialogueBlueprint->PostEditChange();
			DialogueBlueprint->MarkPackageDirty();
		}
	}

}

void FDialogueGraphEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
	}

	FEditorUndoClient::PostRedo(bSuccess);

	// Update UI
	if (DialogueBlueprint->DialogueGraph)
	{
		DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
		if (DialogueBlueprint)
		{
			DialogueBlueprint->PostEditChange();
			DialogueBlueprint->MarkPackageDirty();
		}
	}
}


void FDialogueGraphEditor::CreateDefaultCommands()
{
	FBlueprintEditor::CreateDefaultCommands();
}

UBlueprint* FDialogueGraphEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UDialogueBlueprint>()) { return (UBlueprint*)EditingObjs[i]; }
	}
	return nullptr;
}

//void FDialogueGraphEditor::StartEditingDefaults(bool bAutoFocus /*= true*/, bool bForceRefresh /*= false*/)
//{
//	if (DialogueBlueprint && DialogueBlueprint->DialogueTemplate)
//	{
//		UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();
//
//		// Update the details panel
//		FString Title;
//		DefaultObject->GetName(Title);
//		SKismetInspector::FShowDetailsOptions Options(FText::FromString(Title), bForceRefresh);
//		Options.bShowComponents = false;
//
//		Inspector->ShowDetailsForSingleObject(DialogueBlueprint->DialogueTemplate, Options);
//	}
//	else
//	{
//		FBlueprintEditor::StartEditingDefaults(bAutoFocus, bForceRefresh);
//	}
//}

void FDialogueGraphEditor::OnAddInputPin()
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = UpdateGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		//UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		//if (LogicNode)
		//{
		//	const FScopedTransaction Transaction(LOCTEXT("AddInputPin", "Add Input Pin"));

		//	LogicNode->Modify();
		//	LogicNode->AddInputPin();

		//	const UEdGraphSchema* Schema = LogicNode->GetSchema();
		//	Schema->ReconstructNode(*LogicNode);
		//}
	}

	// Refresh the current graph, so the pins can be updated
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FDialogueGraphEditor::CanAddInputPin() const
{
	return true;
}

void FDialogueGraphEditor::OnRemoveInputPin()
{

}

bool FDialogueGraphEditor::CanRemoveInputPin() const
{
	return true;
}

FText FDialogueGraphEditor::GetDialogueEditorTitle() const
{
	return FText::FromString(GetNameSafe(DialogueBlueprint));
}

FGraphAppearanceInfo FDialogueGraphEditor::GetDialogueGraphAppearance() const
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NARRATIVE DIALOGUE EDITOR");
	return AppearanceInfo;
}

bool FDialogueGraphEditor::InEditingMode(bool bGraphIsEditable) const
{
	return bGraphIsEditable;
}

bool FDialogueGraphEditor::CanAccessDialogueEditorMode() const
{
	return IsValid(DialogueBlueprint);
}

FText FDialogueGraphEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(DialogueEditorMode, LOCTEXT("DialogueEditorMode", "Dialogue Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	return *OutDesc;
}


UDialogueBlueprint* FDialogueGraphEditor::GetDialogueAsset() const
{
	return DialogueBlueprint;
}

TSharedRef<SWidget> FDialogueGraphEditor::SpawnProperties()
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			DetailsView.ToSharedRef()
		];
}

void FDialogueGraphEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::RestoreDialogueGraph()
{
	UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);
	const bool bNewGraph = MyGraph == NULL;
	if (MyGraph == NULL)
	{
		//if (DialogueBlueprint->LegacyAsset)
		//{
		//	DialogueBlueprint->LegacyAsset->DialogueGraph->Rename(nullptr, DialogueBlueprint);
		//	DialogueBlueprint->DialogueGraph = DialogueBlueprint->LegacyAsset->DialogueGraph;
		//	MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);
		//}
		//else
		//{
			DialogueBlueprint->DialogueGraph = FBlueprintEditorUtils::CreateNewGraph(DialogueBlueprint, TEXT("Dialogue Graph"), UDialogueGraph::StaticClass(), UDialogueGraphSchema::StaticClass());
			MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);

			FBlueprintEditorUtils::AddUbergraphPage(DialogueBlueprint, MyGraph);

			// Initialize the behavior tree graph
			const UEdGraphSchema* Schema = MyGraph->GetSchema();
			Schema->CreateDefaultNodesForGraph(*MyGraph);

			MyGraph->OnCreated();
		//}
	}
	else
	{
		MyGraph->OnLoaded();
	}

	MyGraph->Initialize();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(MyGraph);
	TSharedPtr<SDockTab> DocumentTab = DocumentManager->OpenDocument(Payload, bNewGraph ? FDocumentTracker::OpenNewDocument : FDocumentTracker::RestorePreviousDocument);

	if (DialogueBlueprint->LastEditedDocuments.Num() > 0)
	{
		TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DocumentTab->GetContent());
		GraphEditor->SetViewLocation(DialogueBlueprint->LastEditedDocuments[0].SavedViewOffset, DialogueBlueprint->LastEditedDocuments[0].SavedZoomAmount);
	}
}

void FDialogueGraphEditor::SaveEditedObjectState()
{
	DialogueBlueprint->LastEditedDocuments.Empty();
	DocumentManager->SaveAllState();
}

TSharedRef<class SGraphEditor> FDialogueGraphEditor::CreateDialogueGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph);

	CreateDialogueCommandList();

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FDialogueGraphEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDialogueGraphEditor::OnNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FDialogueGraphEditor::OnNodeTitleCommitted);

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget =
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Graph.TitleBackground")))
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(this, &FDialogueGraphEditor::GetDialogueEditorTitle)
			.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
		]
		];

	// Make full graph editor
	const bool bGraphIsEditable = InGraph->bEditable;
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FDialogueGraphEditor::InEditingMode, bGraphIsEditable)
		.Appearance(this, &FDialogueGraphEditor::GetDialogueGraphAppearance)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);

}

void FDialogueGraphEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = false;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(NULL);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FDialogueGraphEditor::OnFinishedChangingProperties);
}

void FDialogueGraphEditor::ExtendMenu()
{
}


void FDialogueGraphEditor::ExtendToolbar()
{

}

//void FDialogueGraphEditor::BindCommonCommands()
//{
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ShowDialogueDetails,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::ShowDialogueDetails),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanShowDialogueDetails));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ViewTutorial,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().QuickAddNode,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::QuickAddNode),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanQuickAddNode));
//}

#undef LOCTEXT_NAMESPACE