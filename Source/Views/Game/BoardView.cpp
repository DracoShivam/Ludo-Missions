#include "Views/Game/BoardView.h"

#include <map>

#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Events/UiEvents.h"
#include "Models/BoardLayout.h"
#include "Views/Common/UiFactory.h"
#include "Views/Game/BoardGeometry.h"
#include "Views/Game/RollChoiceView.h"
#include "Views/Game/TokenView.h"

namespace lm {

using ui::CELL;
using ui::gridToLocal;

static const ax::Color4B WHITE(255, 255, 255, 255);
static const ax::Color4B GRID_LINE(150, 150, 160, 255);

bool BoardView::init() {
	if (!Node::init()) {
		return false;
	}
	for (auto& row : m_progress) row.fill(IN_YARD);
	drawBoard();
	m_tokenLayer = ax::Node::create();
	addChild(m_tokenLayer, 10);
	for (int p = 0; p < 4; p++) {
		for (int t = 0; t < 4; t++) {
			auto* tv = TokenView::create(p, t);
			m_tokens[p][t] = tv;
			m_tokenLayer->addChild(tv);
		}
	}
	placeAll();

	auto* touch = ax::EventListenerTouchOneByOne::create();
	touch->setSwallowTouches(false);
	touch->onTouchBegan = [](ax::Touch*, ax::Event*) { return true; };
	touch->onTouchEnded = [this](ax::Touch* t, ax::Event*) {
		ax::Vec2 local = convertToNodeSpace(t->getLocation());
		if (auto* tv = tokenAt(local)) {
			UiTokenTapped e;
			e.player = tv->player();
			e.token = tv->token();
			EventBus::publish(e);
		} else {
			closeChoice();
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
	return true;
}

void BoardView::drawBoard() {
	float half = ui::BOARD_SIZE / 2;
	auto* frame = ui::makePanel(ax::Size(ui::BOARD_SIZE + 20, ui::BOARD_SIZE + 20), ax::Color3B(245, 240, 230));
	addChild(frame, -1);

	auto* dn = ax::DrawNode::create();
	addChild(dn, 0);
	auto cellRect = [&](GridPos g, ax::Color4B fill, float shrink = 0.94f) {
		ax::Vec2 c = gridToLocal(g);
		float h = CELL * shrink / 2;
		dn->drawSolidRect(c - ax::Vec2(h, h), c + ax::Vec2(h, h), fill, 1.f, GRID_LINE);
	};
	dn->drawSolidRect(ax::Vec2(-half, -half), ax::Vec2(half, half), WHITE);

	// Yards
	for (int p = 0; p < 4; p++) {
		ax::Vec2 c = gridToLocal(board::yardCenterGrid(p));
		float h = CELL * 3 - 2;
		dn->drawSolidRect(c - ax::Vec2(h, h), c + ax::Vec2(h, h), ui::playerColor4B(p));
		float hi = CELL * 2;
		dn->drawSolidRect(c - ax::Vec2(hi, hi), c + ax::Vec2(hi, hi), WHITE);
		for (int t = 0; t < 4; t++) {
			dn->drawSolidCircle(gridToLocal(board::yardSpotGrid(p, t)), CELL * 0.4f, 0, 32, ui::playerColor4B(p, 110));
		}
	}
	// Track
	for (int i = 0; i < TRACK_LEN; i++) {
		bool isStart = (i % 13 == 0);
		cellRect(board::trackGrid(i), isStart ? ui::playerColor4B(i / 13) : WHITE);
		if (board::isSafeCell(i) && !isStart) {
			auto* star = ax::Sprite::create(ui::IMG_STAR);
			star->setScale(CELL * 0.8f / star->getContentSize().width);
			star->setColor(ax::Color3B(150, 150, 160));
			star->setPosition(gridToLocal(board::trackGrid(i)));
			addChild(star, 1);
		}
	}
	// Home lanes
	for (int p = 0; p < 4; p++) {
		for (int k = 0; k < 5; k++) {
			cellRect(board::homeLaneGrid(p, k), ui::playerColor4B(p));
		}
	}
	// Centre triangles (RED bottom, GREEN left, YELLOW top, BLUE right)
	ax::Vec2 ctr = gridToLocal({7, 7});
	ax::Vec2 bl = gridToLocal({5.5f, 5.5f}), br = gridToLocal({8.5f, 5.5f});
	ax::Vec2 tr = gridToLocal({8.5f, 8.5f}), tl = gridToLocal({5.5f, 8.5f});
	ax::Vec2 tris[4][3] = {{bl, br, ctr}, {tl, bl, ctr}, {tr, tl, ctr}, {br, tr, ctr}};
	for (int p = 0; p < 4; p++) {
		dn->drawSolidPoly(tris[p], 3, ui::playerColor4B(p), 1.f, GRID_LINE, true);
	}
}

void BoardView::initListeners() {
	EventBus::subscribe<MatchSnapshot>(this, [this](const MatchSnapshot& s) {
		m_timing = s.timing;
		m_self = s.state.selfPlayer;
		for (int p = 0; p < 4; p++) m_progress[p] = s.state.players[p].progress;
		clearHighlights();
		closeChoice();
		placeAll();
	});
	EventBus::subscribe<GameEventMsg>(this, [this](const GameEventMsg& m) {
		switch (m.event.type) {
			case GameEventType::TOKEN_MOVED:
				clearHighlights();
				closeChoice();
				animateMove(m.event, m.animScale);
				break;
			case GameEventType::TOKEN_CAPTURED:
			// A kick sends a token home exactly as a capture does, and carries the same victim
			// fields. Without this case it fell to `default:` and the board simply never redrew --
			// which is why Send Home looked like it did nothing at all.
			case GameEventType::TOKEN_KICKED:
				animateCapture(m.event, m.animScale);
				break;
			case GameEventType::TURN_STARTED:
			case GameEventType::MATCH_ENDED:
				clearHighlights();
				closeChoice();
				break;
			default:
				break;
		}
	});
	EventBus::subscribe<TappableTokens>(this, [this](const TappableTokens& m) {
		clearHighlights();
		bool danger = m.reason == TappableTokens::Reason::PowerTarget;
		for (const auto& t : m.tokens) {
			if (t.player < 0 || t.player >= NUM_PLAYERS || t.token < 0 || t.token >= TOKENS_PER_PLAYER) continue;
			m_tokens[t.player][t.token]->setHighlighted(true, danger);
		}
	});
	EventBus::subscribe<RollChoiceRequested>(this, [this](const RollChoiceRequested& r) {
		closeChoice();
		m_choice = RollChoiceView::create(r.player, r.token, r.values);
		m_choice->setPosition(m_tokens[r.player][r.token]->getPosition() + ax::Vec2(0, CELL * 1.2f));
		addChild(m_choice, 50);
	});
}

ax::Vec2 BoardView::basePosition(int player, int token) const {
	return gridToLocal(board::gridForToken(player, token, m_progress[player][token]));
}

void BoardView::placeAll() {
	for (int p = 0; p < 4; p++) {
		for (int t = 0; t < 4; t++) {
			m_tokens[p][t]->stopAllActions();
			m_tokens[p][t]->moving = false;
		}
	}
	relayout();
}

// Positions all non-moving tokens; tokens sharing a spot are shrunk and fanned out.
void BoardView::relayout() {
	std::map<std::pair<int, int>, std::vector<TokenView*>> groups;
	for (int p = 0; p < 4; p++) {
		for (int t = 0; t < 4; t++) {
			TokenView* tv = m_tokens[p][t];
			if (tv->moving) continue;
			ax::Vec2 pos = basePosition(p, t);
			groups[{(int) std::lround(pos.x), (int) std::lround(pos.y)}].push_back(tv);
		}
	}
	static const ax::Vec2 OFF[4] = {{-8, -6}, {8, -6}, {-8, 8}, {8, 8}};
	for (auto& [key, list] : groups) {
		ax::Vec2 base((float) key.first, (float) key.second);
		bool stacked = list.size() > 1;
		for (size_t i = 0; i < list.size(); i++) {
			list[i]->setPosition(base + (stacked ? OFF[i % 4] : ax::Vec2::ZERO));
			list[i]->setScale(stacked ? 0.75f : 1.f);
			list[i]->setLocalZOrder((int) (-base.y) + (int) i);
		}
	}
}

void BoardView::animateMove(const GameEvent& e, float animScale) {
	TokenView* tv = m_tokens[e.player][e.token];
	tv->stopAllActions();
	tv->moving = true;
	tv->setScale(1.f);
	tv->setLocalZOrder(1000);
	m_progress[e.player][e.token] = e.to;

	ax::Vector<ax::FiniteTimeAction*> steps;
	float stepDur = m_timing.tokenStep * animScale;
	std::vector<int> path;
	if (e.from == IN_YARD) {
		path.push_back(0);
	} else if (e.to < e.from || e.to - e.from > MAX_ROLL_VALUE) {
		// Powers move tokens in ways dice cannot: Swap sends one backwards, Jump Home hurls one
		// most of the way round. Stepping cell by cell would either produce an EMPTY path (backwards,
		// so the token never visibly moves) or ~47 steps at 0.18s each, freezing the board for eight
		// seconds. Glide instead -- it reads as the teleport it is.
		path.push_back(e.to);
	} else {
		for (int p = e.from + 1; p <= e.to; p++) path.push_back(p);
	}
	for (int p : path) {
		ax::Vec2 target = gridToLocal(board::gridForToken(e.player, e.token, p));
		steps.pushBack(ax::EaseSineOut::create(ax::MoveTo::create(stepDur, target)));
		steps.pushBack(ax::CallFunc::create([] { ui::playSound(ui::SND_MOVE); }));
	}
	steps.pushBack(ax::CallFunc::create([this, tv] {
		tv->moving = false;
		relayout();
	}));
	tv->runAction(ax::Sequence::create(steps));
}

void BoardView::animateCapture(const GameEvent& e, float animScale) {
	TokenView* victim = m_tokens[e.victimPlayer][e.victimToken];
	victim->stopAllActions();
	victim->moving = true;
	victim->setScale(1.f);
	victim->setLocalZOrder(999);
	m_progress[e.victimPlayer][e.victimToken] = IN_YARD;
	ui::playSound(ui::SND_KILL);
	ax::Vec2 home = gridToLocal(board::yardSpotGrid(e.victimPlayer, e.victimToken));
	float d = m_timing.captureAnim * animScale;
	victim->runAction(ax::Sequence::create(ax::JumpTo::create(d, home, CELL * 1.5f, 1), ax::CallFunc::create([this, victim] {
		victim->moving = false;
		relayout();
	}), nullptr));
}

void BoardView::clearHighlights() {
	for (auto& row : m_tokens)
		for (auto* tv : row) tv->setHighlighted(false);
}

void BoardView::closeChoice() {
	if (m_choice) {
		m_choice->removeFromParent();
		m_choice = nullptr;
	}
}

TokenView* BoardView::tokenAt(const ax::Vec2& localPoint) const {
	ax::Vec2 inLayer = m_tokenLayer->convertToNodeSpace(convertToWorldSpace(localPoint));
	TokenView* best = nullptr;
	for (auto& row : m_tokens) {
		for (auto* tv : row) {
			if (tv->isHighlighted() && tv->hitTest(inLayer)) {
				if (!best || tv->getLocalZOrder() > best->getLocalZOrder()) best = tv;
			}
		}
	}
	return best;
}

}  // namespace lm
