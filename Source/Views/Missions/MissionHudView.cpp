#include "Views/Missions/MissionHudView.h"

#include "Events/EventBus.h"
#include "Events/MissionEvents.h"
#include "Views/Common/ToastView.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"
#include "Views/Missions/MissionCardView.h"

namespace lm {

MissionHudView* MissionHudView::create(ToastView* toasts, std::function<ax::Vec2()> coinTargetWorld) {
	auto* v = new (std::nothrow) MissionHudView();
	if (v && v->initWith(toasts, std::move(coinTargetWorld))) {
		v->autorelease();
		return v;
	}
	AX_SAFE_DELETE(v);
	return nullptr;
}

bool MissionHudView::initWith(ToastView* toasts, std::function<ax::Vec2()> coinTargetWorld) {
	if (!Node::init()) return false;
	m_toasts = toasts;
	m_coinTarget = std::move(coinTargetWorld);
	m_empty = ui::makeLabel("Missions appear here at the right moments", 18, false, ax::Color3B(140, 150, 190));
	m_empty->setPosition(ui::DESIGN_W / 2, 0);
	addChild(m_empty);
	return true;
}

int MissionHudView::freeSlot() const {
	for (int s = 0; s < 3; s++) {
		bool used = false;
		for (auto& [uid, slot] : m_slotOf) used |= slot == s;
		if (!used) return s;
	}
	return -1;
}

void MissionHudView::initListeners() {
	EventBus::subscribe<MissionUpdated>(this, [this](const MissionUpdated& ev) {
		const MissionInstance& m = ev.update.instance;
		auto it = m_cards.find(m.uid);
		switch (ev.update.kind) {
			case MissionUpdate::Kind::Offered: {
				int slot = freeSlot();
				if (slot < 0) return;
				auto* card = MissionCardView::create(m);
				card->setPosition(ui::HUD_CARD_X[slot], 0);
				addChild(card);
				m_cards[m.uid] = card;
				m_slotOf[m.uid] = slot;
				if (m_toasts) m_toasts->show("New mission: " + m.title, ax::Color3B(70, 90, 170));
				break;
			}
			case MissionUpdate::Kind::Progress:
				if (it != m_cards.end()) it->second->update(m);
				break;
			case MissionUpdate::Kind::Completed:
				if (it != m_cards.end()) {
					it->second->update(m);
					coinFly(it->second->rewardWorldPosition(), m.rewardCoins);
					it->second->playCompleted();
				}
				if (m_toasts) m_toasts->show("Mission complete!  +" + std::to_string(m.rewardCoins), ax::Color3B(40, 160, 80));
				break;
			case MissionUpdate::Kind::Failed:
				if (it != m_cards.end()) it->second->playFailed();
				if (m_toasts) m_toasts->show("Mission failed", ax::Color3B(170, 60, 60));
				break;
			case MissionUpdate::Kind::Voided:
				if (it != m_cards.end()) it->second->removeFromParent();
				break;
		}
		if (ev.update.kind != MissionUpdate::Kind::Offered && ev.update.kind != MissionUpdate::Kind::Progress) {
			m_cards.erase(m.uid);  // the card removes itself after its animation
			m_slotOf.erase(m.uid);
		}
		m_empty->setVisible(m_cards.empty());
	});
}

void MissionHudView::coinFly(ax::Vec2 fromWorld, int coins) {
	if (!m_coinTarget || !getParent()) return;
	ax::Node* layer = getParent();  // fly in the scene's space so coins aren't clipped by the HUD
	ax::Vec2 from = layer->convertToNodeSpace(fromWorld);
	ax::Vec2 to = layer->convertToNodeSpace(m_coinTarget());
	int n = std::min(8, std::max(3, coins / 10));
	for (int i = 0; i < n; i++) {
		auto* c = ax::Sprite::create(ui::IMG_COIN);
		c->setScale(28.f / c->getContentSize().width);
		c->setPosition(from + ax::Vec2((float) (i % 3 - 1) * 10, (float) (i / 3) * 6));
		layer->addChild(c, 500);
		ax::Vec2 mid = (from + to) / 2 + ax::Vec2(-60 + 30.f * (i % 4), 80);
		ax::ccBezierConfig bz;
		bz.controlPoint_1 = mid;
		bz.controlPoint_2 = mid;
		bz.endPosition = to;
		c->runAction(ax::Sequence::create(ax::DelayTime::create(0.06f * i), ax::EaseSineIn::create(ax::BezierTo::create(0.6f, bz)),
										  ax::CallFunc::create([i] {
											  if (i == 0) ui::playSound(ui::SND_COIN);
										  }),
										  ax::RemoveSelf::create(), nullptr));
	}
}

}  // namespace lm
