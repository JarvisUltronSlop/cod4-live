#pragma once
struct UiContext;
namespace live::controller_ui {
void BeforeInitialize();
void Initialize(UiContext* context);
void MenusLoaded(UiContext* context);
void Refresh(UiContext* context);
void Open();
void Dump();
bool IsOpen();
bool HandlesKey(int key);
void ConfirmBackend();
void CancelBackend();
int RequestedBackend();
void ObserveBackend(bool present);
}
