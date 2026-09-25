#include "Views/Game/GameScene.h"

#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Events/MissionEvents.h"
#include "Events/UiEvents.h"
#include "Views/Common/CoinCounterView.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"
#include "Views/Game/BoardView.h"
#include "Views/Game/DebugOverlayView.h"
#include "Views/Game/PlayerPanelView.h"
#include "Views/Game/ResultPopup.h"
#include "Views/Common/ToastView.h"
#include "Views/Missions/MissionHudView.h"
#include "Views/Powers/PowerTrayView.h"
#include "Views/Powers/TargetBannerView.h"

namespace lm {

bool GameScene::init() {
	if (!Scene::init()) {
		return false;
	}
	auto vo = ui::visibleOrigin();
	auto vs = ui::visibleSize();
	float cx = vo.x + vs.width / 2;
	addChild(ax::LayerColor::create(ui::BG_COLOR));

	// Top bar
	auto* coins = ax::utils::createInstance<CoinCounterView>();
	coins->setName("coins");
	coins->setPosition(vo.x + vs.width - 110, ui::topY(ui::TOP_BAR_FROM_TOP));
	addChild(coins, 20);
	auto* back = ui::makeButton("< Lobby", ax::Size(150, 56), [] { EventBus::publish(UiResultClosed{false}); }, ax::Color3B(90, 110, 170));
	back->setPosition(ax::Vec2(vo.x + 95, ui::topY(ui::TOP_BAR_FROM_TOP)));
	addChild(back, 20);

	// Board, centred slightly below the visible middle; panels hug its top/bottom edges
	float boardY = vo.y + vs.height * (ui::BOARD_CENTER_Y / ui::DESIGN_H);
	auto* board = ax::utils::createInstance<BoardView>();
	board->setPosition(cx, boardY);
	addChild(board, 5);

	float half = ui::BOARD_SIZE / 2;
	float topPanelY = boardY + half + 18 + ui::PANEL_H / 2;
	float bottomPanelY = boardY - half - 18 - ui::PANEL_H / 2;
	struct Slot {
		float x, y;
		bool diceOnRight;
	};
	const Slot slots[4] = {{ui::PANEL_LEFT_X, bottomPanelY, true},
						   {ui::PANEL_LEFT_X, topPanelY, true},
						   {ui::PANEL_RIGHT_X, topPanelY, false},
						   {ui::PANEL_RIGHT_X, bottomPanelY, false}};
	for (int p = 0; p < 4; p++) {
		auto* panel = PlayerPanelView::create(p, slots[p].diceOnRight);
		panel->setPosition(vo.x + slots[p].x, slots[p].y);
		addChild(panel, 6);
	}

	// Mission plug-in UI: toasts over the board, HUD cards under the top bar
	auto* toasts = ax::utils::createInstance<ToastView>();
	toasts->setPosition(cx, boardY + 150);
	addChild(toasts, 150);
	auto* hud = MissionHudView::create(toasts, [coins] { return coins->iconWorldPosition(); });
	hud->setPosition(vo.x, ui::topY(ui::HUD_FROM_TOP));
	addChild(hud, 15);

	// Powers are a shipping feature, not a dev tool: this used to sit inside #if LM_DEV, so there
	// was no tray at all on iOS. Centred on `cx` like everything else, not on DESIGN_W / 2.
	auto* tray = PowerTrayView::create();
	tray->setPosition(cx, ui::POWER_TRAY_Y);
	addChild(tray, 16);

	auto* banner = TargetBannerView::create();
	banner->setPosition(cx, ui::POWER_BANNER_Y);
	addChild(banner, 140);

#if defined(LM_DEV) && LM_DEV
	auto* dbg = ax::utils::createInstance<DebugOverlayView>();
	dbg->setPosition(vo.x + 8, vo.y + 6);
	addChild(dbg, 100);
#endif
	return true;
}

void GameScene::initListeners() {
	EventBus::subscribe<MatchSnapshot>(this, [this](const MatchSnapshot& s) {
		m_names = s.names;
		m_self = s.state.selfPlayer;
		m_resultDelay = s.timing.resultPopupDelay;
		m_missionsCompleted = 0;
		m_coinsEarned = 0;
	});
	EventBus::subscribe<GameEventMsg>(this, [this](const GameEventMsg& m) {
		if (m.event.type == GameEventType::TURN_STARTED && m.event.player == m_self) {
			ui::playSound(ui::SND_MY_TURN);
		}
		if (m.event.type == GameEventType::MATCH_ENDED) {
			if (m.event.player == m_self) ui::playSound(ui::SND_WIN);
			runAction(ax::Sequence::create(ax::DelayTime::create(m_resultDelay), ax::CallFunc::create([this] { showResult(); }), nullptr));
		}
	});
	EventBus::subscribe<MissionMatchSummary>(this, [this](const MissionMatchSummary& s) {
		m_missionsCompleted = s.completed;
		m_coinsEarned = s.coinsEarned;
	});
	EventBus::subscribe<MatchRanking>(this, [this](const MatchRanking& r) { m_ranking = r.ranking; });
}

void GameScene::showResult() {
	ResultData d;
	d.ranking = m_ranking;
	d.names = m_names;
	d.self = m_self;
	d.missionsCompleted = m_missionsCompleted;
	d.coinsEarned = m_coinsEarned;
	addChild(ResultPopup::create(d), 200);
}

void GameScene::afterTransition() {
	EventBus::publish(UiGameSceneReady{});
}

void GameScene::beforeExit() {
	EventBus::publish(UiGameSceneExiting{});
}

}  // namespace lm
