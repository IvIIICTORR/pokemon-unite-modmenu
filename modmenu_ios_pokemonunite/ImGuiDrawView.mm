//
// ImGuiDrawView.mm - Pokemon Unite Mod Menu (iOS)
// Baseado no template 34306/HuyJIT-ModMenu (ImGui + Metal + DobbyHook + resolver il2cpp)
// Adaptado para Pokemon Unite: Map Hack + Aimbot de skills
//
// Alvo: jp.pokemon.pokemonunite / UnityFramework
//

//Require standard library
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Foundation/Foundation.h>
//Imgui library
#import "Esp/CaptainHook.h"
#import "Esp/ImGuiDrawView.h"
#import "IMGUI/imgui.h"
#import "IMGUI/imgui_impl_metal.h"
#import "IMGUI/zzz.h"
//Patch / hook library (vindos do template HuyJIT via CI)
#import "5Toubun/NakanoIchika.h"
#import "5Toubun/NakanoNino.h"
#import "5Toubun/NakanoMiku.h"
#import "5Toubun/NakanoYotsuba.h"
#import "5Toubun/NakanoItsuki.h"
#import "5Toubun/dobby.h"
#import "5Toubun/il2cpp.h"
//Offsets do Pokemon Unite
#import "PokemonUnite.h"

#define kWidth  [UIScreen mainScreen].bounds.size.width
#define kHeight [UIScreen mainScreen].bounds.size.height
#define kScale [UIScreen mainScreen].scale
#define patch_NULL(a, b) vm(ENCRYPTOFFSET(a), strtoul(ENCRYPTHEX(b), nullptr, 0))
#define patch(a, b) vm_unity(ENCRYPTOFFSET(a), strtoul(ENCRYPTHEX(b), nullptr, 0))


@interface ImGuiDrawView () <MTKViewDelegate>
@property (nonatomic, strong) id <MTLDevice> device;
@property (nonatomic, strong) id <MTLCommandQueue> commandQueue;
@end

@implementation ImGuiDrawView

// ======================= TOGGLES =======================
// Todos comecam DESLIGADOS. Os hooks so sao instalados quando a feature e ligada
// (instalacao preguicosa). Com tudo off, NENHUM hook e instalado -> jogo roda limpo.
static bool bMapHack = false;   // Visao total (anti-Fog of War)
static bool bAntiCheat = false; // Neutraliza UpdateToCheckVisionStat
static bool bAimbot = false;    // Forca auto-select de alvo nas skills

// Estado de instalacao (para status no menu)
static bool gInstalledMapHack = false;
static bool gInstalledAntiCheat = false;
static bool gInstalledAimbot = false;

// ======================= MAP HACK HOOKS =======================
// Todas retornam "visivel = true" quando bMapHack esta ligado.
// Usar hook (nao patch estatico) permite setar o out bool do CheckVisible,
// o que corrige o bug de auto-attack quebrado do patch estatico.

// 1) static bool MVisionSysUtil.IsVisible(Actor, Actor, bool)
bool (*o_MVisionSysUtil_IsVisible)(void *actor, void *checkedActor, bool checkAntiInvisible);
bool hk_MVisionSysUtil_IsVisible(void *actor, void *checkedActor, bool checkAntiInvisible) {
    if (bMapHack) return true;
    return o_MVisionSysUtil_IsVisible(actor, checkedActor, checkAntiInvisible);
}

// 2) static bool LVisionSysUtil.IsCampVisible(LVisionSysComp, int, int)
bool (*o_LVisionSysUtil_IsCampVisible)(void *sysComp, int srcCamp, int dstId);
bool hk_LVisionSysUtil_IsCampVisible(void *sysComp, int srcCamp, int dstId) {
    if (bMapHack) return true;
    return o_LVisionSysUtil_IsCampVisible(sysComp, srcCamp, dstId);
}

// 3) bool LVisionSys.CheckVisible(LVisionSysComp, Entity, Entity, out bool)  [instancia]
bool (*o_LVisionSys_CheckVisible)(void *self, void *comp, void *src, void *dst, bool *visible);
bool hk_LVisionSys_CheckVisible(void *self, void *comp, void *src, void *dst, bool *visible) {
    if (bMapHack) {
        if (visible) *visible = true;   // CRUCIAL: seta o out param (evita quebrar auto-attack)
        return true;
    }
    return o_LVisionSys_CheckVisible(self, comp, src, dst, visible);
}

// 4) bool LVisionSys.IsCampVisible(LVisionSysComp, int, Entity)  [instancia]
bool (*o_LVisionSys_IsCampVisible)(void *self, void *comp, int camp, void *dstEntity);
bool hk_LVisionSys_IsCampVisible(void *self, void *comp, int camp, void *dstEntity) {
    if (bMapHack) return true;
    return o_LVisionSys_IsCampVisible(self, comp, camp, dstEntity);
}

// 5) void LBattleStatSys.UpdateToCheckVisionStat()  [anti-cheat]
void (*o_LBattleStatSys_UpdateStat)(void *self);
void hk_LBattleStatSys_UpdateStat(void *self) {
    if (bAntiCheat) return;   // nao coleta estatisticas de visao suspeitas
    o_LBattleStatSys_UpdateStat(self);
}

// ======================= AIMBOT HOOK =======================
// Forca o auto-select de alvo do proprio jogo (bIsAutoSelectTarget = true),
// fazendo skills direcionais mirarem automaticamente no melhor/mais proximo alvo.
// Entity SelectTargetByDir(Entity inActor, LSkillSlot inUseSlot, bool bIsAutoSelectTarget, VInt2 dir)
void *(*o_LSkill_SelectTargetByDir)(void *self, void *inActor, void *inUseSlot, bool bAuto, struct VInt2 dir);
void *hk_LSkill_SelectTargetByDir(void *self, void *inActor, void *inUseSlot, bool bAuto, struct VInt2 dir) {
    if (bAimbot) bAuto = true;
    return o_LSkill_SelectTargetByDir(self, inActor, inUseSlot, bAuto, dir);
}

// ======================= INSTALACAO PREGUICOSA (por feature) =======================
// Cada grupo so e instalado quando a feature correspondente e ligada pela 1a vez.
// Assim isolamos culpados: se o crash sumir com tudo off, o problema esta em um hook.
// Habilite UMA feature por vez para descobrir qual causa o crash na partida.

static void install_maphack_hooks() {
    static dispatch_once_t t;
    dispatch_once(&t, ^{
        // Utility estaticas (mais seguras)
        DobbyHook((void *)getRealOffset(OFF_MVisionSysUtil_IsVisible),
                  (void *)hk_MVisionSysUtil_IsVisible, (void **)&o_MVisionSysUtil_IsVisible);
        DobbyHook((void *)getRealOffset(OFF_LVisionSysUtil_IsCampVisible),
                  (void *)hk_LVisionSysUtil_IsCampVisible, (void **)&o_LVisionSysUtil_IsCampVisible);
        // Metodos de instancia (suspeitos de instabilidade — ver notas de teste)
        DobbyHook((void *)getRealOffset(OFF_LVisionSys_CheckVisible),
                  (void *)hk_LVisionSys_CheckVisible, (void **)&o_LVisionSys_CheckVisible);
        DobbyHook((void *)getRealOffset(OFF_LVisionSys_IsCampVisible),
                  (void *)hk_LVisionSys_IsCampVisible, (void **)&o_LVisionSys_IsCampVisible);
        gInstalledMapHack = true;
    });
}

static void install_anticheat_hooks() {
    static dispatch_once_t t;
    dispatch_once(&t, ^{
        DobbyHook((void *)getRealOffset(OFF_LBattleStatSys_UpdateStat),
                  (void *)hk_LBattleStatSys_UpdateStat, (void **)&o_LBattleStatSys_UpdateStat);
        gInstalledAntiCheat = true;
    });
}

static void install_aimbot_hooks() {
    static dispatch_once_t t;
    dispatch_once(&t, ^{
        DobbyHook((void *)getRealOffset(OFF_LSkill_SelectTargetByDir),
                  (void *)hk_LSkill_SelectTargetByDir, (void **)&o_LSkill_SelectTargetByDir);
        gInstalledAimbot = true;
    });
}

static bool MenDeal = true;


- (instancetype)initWithNibName:(nullable NSString *)nibNameOrNil bundle:(nullable NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];

    _device = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];

    if (!self.device) abort();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsClassic();

    ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF((void*)zzz_compressed_data, zzz_compressed_size, 60.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    ImGui_ImplMetal_Init(_device);

    return self;
}

+ (void)showChange:(BOOL)open
{
    MenDeal = open;
}

- (MTKView *)mtkView
{
    return (MTKView *)self.view;
}

- (void)loadView
{
    CGFloat w = [UIApplication sharedApplication].windows[0].rootViewController.view.frame.size.width;
    CGFloat h = [UIApplication sharedApplication].windows[0].rootViewController.view.frame.size.height;
    self.view = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, w, h)];
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.mtkView.device = self.device;
    self.mtkView.delegate = self;
    self.mtkView.clearColor = MTLClearColorMake(0, 0, 0, 0);
    self.mtkView.backgroundColor = [UIColor colorWithRed:0 green:0 blue:0 alpha:0];
    self.mtkView.clipsToBounds = YES;
}


#pragma mark - Interaction

- (void)updateIOWithTouchEvent:(UIEvent *)event
{
    UITouch *anyTouch = event.allTouches.anyObject;
    CGPoint touchLocation = [anyTouch locationInView:self.view];
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos = ImVec2(touchLocation.x, touchLocation.y);

    BOOL hasActiveTouch = NO;
    for (UITouch *touch in event.allTouches)
    {
        if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
        {
            hasActiveTouch = YES;
            break;
        }
    }
    io.MouseDown[0] = hasActiveTouch;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self updateIOWithTouchEvent:event]; }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self updateIOWithTouchEvent:event]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self updateIOWithTouchEvent:event]; }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self updateIOWithTouchEvent:event]; }

#pragma mark - MTKViewDelegate

- (void)drawInMTKView:(MTKView*)view
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = view.bounds.size.width;
    io.DisplaySize.y = view.bounds.size.height;

    CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
    io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);
    io.DeltaTime = 1 / float(view.preferredFramesPerSecond ?: 120);

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];

    if (MenDeal == true) {
        [self.view setUserInteractionEnabled:YES];
    } else {
        [self.view setUserInteractionEnabled:NO];
    }

    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil)
    {
        id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder pushDebugGroup:@"ImGui PU"];

        ImGui_ImplMetal_NewFrame(renderPassDescriptor);
        ImGui::NewFrame();

        ImFont* font = ImGui::GetFont();
        font->Scale = 15.f / font->FontSize;

        CGFloat x = (([UIApplication sharedApplication].windows[0].rootViewController.view.frame.size.width) - 400) / 2;
        CGFloat y = (([UIApplication sharedApplication].windows[0].rootViewController.view.frame.size.height) - 320) / 2;

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);

        if (MenDeal == true)
        {
            ImGui::Begin("Pokemon Unite - Mod Menu (Victor)", &MenDeal);
            ImGui::Text("3 dedos, 2 toques = abrir | 2 dedos, 2 toques = ocultar");
            ImGui::Text("Ative ANTES de entrar na partida.");
            ImGui::Separator();

            ImGui::Checkbox("Map Hack (visao total)", &bMapHack);
            ImGui::Checkbox("Anti-Cheat (UpdateToCheckVisionStat -> nop)", &bAntiCheat);
            ImGui::Checkbox("Aimbot de skills (auto-select alvo)", &bAimbot);

            ImGui::Separator();
            ImGui::TextWrapped("DEBUG: ligue UMA feature por vez e teste a partida para achar o crash.");
            ImGui::Text("Hooks instalados: MapHack=%s | AntiCheat=%s | Aimbot=%s",
                        gInstalledMapHack ? "SIM" : "nao",
                        gInstalledAntiCheat ? "SIM" : "nao",
                        gInstalledAimbot ? "SIM" : "nao");
            ImGui::Text("v1.1 | UnityFramework | jp.pokemon.pokemonunite");
            ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
            ImGui::End();

            // Instalacao PREGUICOSA: so instala o hook da feature quando ela e ligada.
            // Com tudo desligado, nenhum hook e instalado -> jogo roda sem crash.
            if (bMapHack)   install_maphack_hooks();
            if (bAntiCheat) install_anticheat_hooks();
            if (bAimbot)    install_aimbot_hooks();
        }

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size
{
}

@end
