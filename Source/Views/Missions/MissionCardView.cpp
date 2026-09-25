#include "Views/Missions/MissionCardView.h"

#include "Views/Common/UiConfig.h"
#include "Views/Common/PowerLook.h"
#include "Views/Common/UiFactory.h"

namespace lm {

static const float W = ui::HUD_CARD_W, H = ui::HUD_CARD_H;
static const float BAR_W = W - 28, BAR_H = 10;

MissionCardView* MissionCardView::create(const MissionInstance& m) {
	auto* v = new (std::nothrow) MissionCardView();
	if (v && v->initWith(m)) {
		v->autorelease();
		return v;
	}
	AX_SAFE_DELETE(v);
	return nullptr;
}

bool MissionCardView::initWith(const MissionInstance& m) {
	if (!Node::init()) return false;
	m_uid = m.uid;
	setCascadeOpacityEnabled(true);
	m_bg = ui::makePanel(ax::Size(W, H), ax::Color3B(52, 64, 108), 245);
	m_bg->setCascadeOpacityEnabled(true);
	addChild(m_bg);

	auto* title = ui::makeLabel(m.title, 20, true, ax::Color3B(255, 225, 120));
	title->setAnchorPoint(ax::Vec2(0, 0.5f));
	title->setPosition(-W / 2 + 12, H / 2 - 18);
	addChild(title);

	m_reward = ax::Node::create();
	m_reward->setCascadeOpacityEnabled(true);
	auto* coin = ax::Sprite::create(ui::IMG_COIN);
	coin->setScale(22.f / coin->getContentSize().width);
	coin->setPosition(-12, 0);
	m_reward->addChild(coin);
	auto* amount = ui::makeLabel("+" + std::to_string(m.rewardCoins), 18, true, ax::Color3B(255, 210, 60));
	amount->setAnchorPoint(ax::Vec2(0, 0.5f));
	amount->setPosition(2, 0);
	m_reward->addChild(amount);
	m_reward->setPosition(W / 2 - 42, H / 2 - 18);
	addChild(m_reward);

	m_desc = ui::makeLabel("", 15, false, ax::Color3B(225, 230, 250));
	m_desc->setDimensions(W - 24, 40);
	m_desc->setAlignment(ax::TextHAlignment::LEFT, ax::TextVAlignment::CENTER);
	// Shrink to fit rather than overflow the box: with VAlignment CENTER an over-long string spills symmetrically
	// into the title above and the progress bar below, which is how a three-line description used to render.
	m_desc->enableWrap(true);
	m_desc->setOverflow(ax::Label::Overflow::SHRINK);
	m_desc->setPosition(0, 6);
	addChild(m_desc);

	m_bar = ax::DrawNode::create();
	// The promised power sits in the footer, between the counter and the turns -- the one gap in that row, and
	// close enough to the reward that the two read as one offer.
	m_power = ui::makeLabel("", 13, true, ax::Color3B::WHITE);
	m_power->setPosition(0, -H / 2 + 12);
	addChild(m_power);

	m_bar->setPosition(-BAR_W / 2, -H / 2 + 26);
	addChild(m_bar);

	m_count = ui::makeLabel("", 14, true);
	m_count->setAnchorPoint(ax::Vec2(0, 0.5f));
	m_count->setPosition(-W / 2 + 12, -H / 2 + 12);
	addChild(m_count);
	m_turns = ui::makeLabel("", 14, false, ax::Color3B(200, 205, 230));
	m_turns->setAnchorPoint(ax::Vec2(1, 0.5f));
	m_turns->setPosition(W / 2 - 12, -H / 2 + 12);
	addChild(m_turns);

	update(m);
	setScale(0.2f);
	runAction(ax::EaseBackOut::create(ax::ScaleTo::create(0.25f, 1.f)));
	return true;
}

void MissionCardView::drawBar(float ratio, ax::Color4B color) {
	m_bar->clear();
	m_bar->drawSolidRect(ax::Vec2(0, 0), ax::Vec2(BAR_W, BAR_H), ax::Color4B(20, 26, 48, 255));
	if (ratio > 0) m_bar->drawSolidRect(ax::Vec2(0, 0), ax::Vec2(BAR_W * std::min(1.f, ratio), BAR_H), color);
}

void MissionCardView::update(const MissionInstance& m) {
	m_desc->setString(m.description);
	// The promised power, shown up front so finishing the mission is worth something visible.
	m_power->setString(m.rewardPower.empty() ? "" : "+ " + m.rewardPowerTitle);
	m_power->setColor(ui::powerTierColor(m.rewardPowerTier));
	m_count->setString(std::to_string(m.progress) + "/" + std::to_string(m.target));
	m_turns->setString(m.turnsLeft == 1 ? "last turn!" : std::to_string(m.turnsLeft) + " turns left");
	m_turns->setColor(m.turnsLeft <= 1 ? ax::Color3B(255, 140, 120) : ax::Color3B(200, 205, 230));
	float target = m.target > 0 ? (float) m.progress / m.target : 0.f;
	float from = m_ratio;
	m_ratio = target;
	stopActionByTag(11);
	auto* tween = ax::ActionFloat::create(0.3f, from, target, [this](float v) { drawBar(v, ax::Color4B(90, 220, 120, 255)); });
	tween->setTag(11);
	runAction(tween);
}

void MissionCardView::playCompleted() {
	m_bg->setColor(ax::Color3B(40, 170, 90));
	drawBar(1.f, ax::Color4B(255, 230, 90, 255));
	runAction(ax::Sequence::create(ax::ScaleTo::create(0.1f, 1.1f), ax::ScaleTo::create(0.1f, 1.f), ax::DelayTime::create(0.6f),
								   ax::Spawn::create(ax::ScaleTo::create(0.2f, 0.2f), ax::FadeOut::create(0.2f), nullptr), ax::RemoveSelf::create(), nullptr));
}

void MissionCardView::playFailed() {
	m_bg->setColor(ax::Color3B(180, 60, 60));
	auto shake = ax::Sequence::create(ax::MoveBy::create(0.05f, ax::Vec2(8, 0)), ax::MoveBy::create(0.05f, ax::Vec2(-16, 0)),
									  ax::MoveBy::create(0.05f, ax::Vec2(8, 0)), nullptr);
	runAction(ax::Sequence::create(shake, ax::DelayTime::create(0.6f), ax::FadeOut::create(0.2f), ax::RemoveSelf::create(), nullptr));
}

ax::Vec2 MissionCardView::rewardWorldPosition() const {
	return convertToWorldSpace(m_reward->getPosition());
}

}  // namespace lm
