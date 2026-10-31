#pragma once
struct UiContext;
namespace live::controller_ui {
void BeforeInitialize();
void Initialize(UiContext* context);
void Refresh(UiContext* context);
void Open();
bool IsOpen();
bool HandlesKey(int key);
}
