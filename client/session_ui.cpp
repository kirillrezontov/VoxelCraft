#include "client/session_environment.hpp"
#if VC_HAS_VOXELUI
#include "client/declarative_ui.hpp"
#endif

namespace vc::client {
bool SessionEnvironment::processUi(SandboxUiController& ui,const Input& frameInput,int width,int height) {
#if VC_HAS_VOXELUI
    if(declarative) return declarative->processGameplay(ui,frameInput,width,height);
#endif
    return ui.process({frameInput.inventory,frameInput.closeUi,frameInput.primaryClick,frameInput.secondaryClick,
        frameInput.material,frameInput.scroll,frameInput.cursorX,frameInput.cursorY,frameInput.settings,frameInput.crafting,frameInput.console,
        frameInput.pause,frameInput.primaryRelease,frameInput.primaryHeld,frameInput.shift,frameInput.text},width,height);
}
void SessionEnvironment::updateUi(const SandboxUiController& ui,bool survival,unsigned health,bool remote,int width,int height) {
#if VC_HAS_VOXELUI
    if(!declarative||screens.current()!=ApplicationScreen::Gameplay) return;
    gameplayUiPrepared=declarative->updateGameplay(ui.screen(),survival,health,remote,width,height,static_cast<float>(ui.settings().uiScale));
    if(gameplayUiPrepared) renderer.setUi(declarative->draw());
    else renderer.clearUi();
#else
    (void)ui;(void)survival;(void)health;(void)remote;(void)width;(void)height;
#endif
}
}
