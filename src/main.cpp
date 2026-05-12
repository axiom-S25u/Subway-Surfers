#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <cstdlib>
#include <cmath>

using namespace geode::prelude;

// 0=none 1=left 2=right 3=up/jump 4=down/slide
static int g_swipe = 0;

class TouchPad : public CCLayer {
public:
    CCPoint startPt;
    bool tracking = false;
    bool fired = false;

    static TouchPad* create(float w, float h) {
        TouchPad* p = new TouchPad();
        if (p && p->initWithSize(w, h)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool initWithSize(float w, float h) {
        if (!CCLayer::init()) return false;
        this->setContentSize(CCSizeMake(w, h));
        this->setAnchorPoint(ccp(0.5f, 0.5f));
        this->ignoreAnchorPointForPosition(false);
        return true;
    }
    void onEnter() override {
        CCLayer::onEnter();
        CCTouchDispatcher::get()->addTargetedDelegate(this, -512, true);
    }
    void onExit() override {
        CCTouchDispatcher::get()->removeDelegate(this);
        CCLayer::onExit();
    }
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        CCPoint loc = this->convertToNodeSpace(t->getLocation());
        CCSize s = this->getContentSize();
        if (loc.x < 0 || loc.y < 0 || loc.x > s.width || loc.y > s.height) return false;
        startPt = t->getLocation();
        tracking = true;
        fired = false;
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!tracking || fired) return;
        CCPoint d = t->getLocation() - startPt;
        float ax = std::abs(d.x), ay = std::abs(d.y);
        float thr = 22.0f;
        if (ax > thr && ax >= ay) {
            g_swipe = d.x > 0 ? 2 : 1;
            fired = true;
        } else if (ay > thr && ay > ax) {
            g_swipe = d.y > 0 ? 3 : 4;
            fired = true;
        }
    }
    void ccTouchEnded(CCTouch* t, CCEvent*) override {
        if (tracking && !fired) {
            // tap = jump
            g_swipe = 3;
        }
        tracking = false;
    }
    void ccTouchCancelled(CCTouch* t, CCEvent*) override {
        tracking = false;
    }
};

class SubwayPopup : public Popup {
protected:
    static constexpr float kPlayW = 240.0f;
    static constexpr float kPlayH = 320.0f;
    static constexpr int kLanes = 3;

    SimplePlayer* shitPlayer = nullptr;
    CCLayerColor* track = nullptr;
    CCLabelBMFont* scoreLbl = nullptr;
    CCLabelBMFont* overLbl = nullptr;
    CCArray* obs = nullptr;
    int lane = 1;
    float targetX = 0.0f;
    float spd = 140.0f;
    float spawnT = 0.0f;
    float spawnIv = 1.05f;
    float scrollOff = 0.0f;
    float score = 0.0f;
    bool alive = true;
    float jumpT = 0.0f;
    float jumpDur = 0.55f;
    float slideT = 0.0f;
    float slideDur = 0.5f;
    TouchPad* pad = nullptr;

    float laneX(int l) {
        float w = kPlayW / (float)kLanes;
        return w * ((float)l + 0.5f);
    }

    bool init(float w, float h) {
        if (!Popup::init(w, h)) return false;
        this->setTitle("Subway Surfers");

        obs = CCArray::create();
        obs->retain();

        CCSize size = m_mainLayer->getContentSize();

        track = CCLayerColor::create(ccc4(22, 24, 38, 255), kPlayW, kPlayH);
        track->ignoreAnchorPointForPosition(false);
        track->setAnchorPoint(ccp(0.5f, 0.5f));
        track->setPosition(ccp(size.width * 0.5f, size.height * 0.5f - 12.0f));
        m_mainLayer->addChild(track, 1);

        for (int i = 1; i < kLanes; i++) {
            float lx = (kPlayW / (float)kLanes) * (float)i;
            CCLayerColor* line = CCLayerColor::create(ccc4(255, 255, 255, 60), 2.0f, kPlayH);
            line->ignoreAnchorPointForPosition(false);
            line->setAnchorPoint(ccp(0.5f, 0.5f));
            line->setPosition(ccp(lx, kPlayH * 0.5f));
            track->addChild(line, 1);
        }

        for (int i = 0; i < 8; i++) {
            CCLayerColor* dash = CCLayerColor::create(ccc4(255, 255, 255, 90), 4.0f, 18.0f);
            dash->ignoreAnchorPointForPosition(false);
            dash->setAnchorPoint(ccp(0.5f, 0.5f));
            dash->setPosition(ccp(kPlayW * 0.5f, (kPlayH / 8.0f) * (float)i + 9.0f));
            dash->setTag(7000 + i);
            track->addChild(dash, 2);
        }

        GameManager* gm = GameManager::sharedState();
        shitPlayer = SimplePlayer::create(0);
        shitPlayer->updatePlayerFrame(gm->getPlayerFrame(), IconType::Cube);
        shitPlayer->setColor(gm->colorForIdx(gm->getPlayerColor()));
        shitPlayer->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
        shitPlayer->updateColors();
        shitPlayer->setScale(0.65f);
        lane = 1;
        targetX = laneX(lane);
        shitPlayer->setPosition(ccp(targetX, 48.0f));
        track->addChild(shitPlayer, 8);

        scoreLbl = CCLabelBMFont::create("0", "bigFont.fnt");
        scoreLbl->setScale(0.45f);
        scoreLbl->setAnchorPoint(ccp(1.0f, 1.0f));
        scoreLbl->setPosition(ccp(kPlayW - 6.0f, kPlayH - 4.0f));
        track->addChild(scoreLbl, 30);

        CCLabelBMFont* hint = CCLabelBMFont::create("swipe / arrows", "chatFont.fnt");
        hint->setScale(0.55f);
        hint->setAnchorPoint(ccp(0.0f, 1.0f));
        hint->setPosition(ccp(6.0f, kPlayH - 4.0f));
        hint->setOpacity(160);
        track->addChild(hint, 30);

        pad = TouchPad::create(kPlayW, kPlayH);
        pad->setPosition(ccp(kPlayW * 0.5f, kPlayH * 0.5f));
        track->addChild(pad, 40);
        g_swipe = 0;

        float legendX = size.width * 0.5f + kPlayW * 0.5f + 30.0f;
        float legendY = size.height * 0.5f + 50.0f;
        addLegendRow(legendX, legendY,         ccc4(70, 160, 230, 255), "BLOCK",   "swap lane");
        addLegendRow(legendX, legendY - 50.0f, ccc4(240, 200, 60, 255), "YELLOW",  "swipe UP");
        addLegendRow(legendX, legendY - 100.0f,ccc4(220, 90, 220, 255), "PURPLE",  "swipe DOWN");

        float ctrlX = size.width * 0.5f - kPlayW * 0.5f - 30.0f;
        float ctrlY = size.height * 0.5f + 50.0f;
        addCtrlLine(ctrlX, ctrlY,         "<- ->",   "lanes");
        addCtrlLine(ctrlX, ctrlY - 50.0f, "swipe UP", "jump");
        addCtrlLine(ctrlX, ctrlY - 100.0f,"swipe DN", "slide");

        this->setKeyboardEnabled(true);
        this->schedule(schedule_selector(SubwayPopup::tick), 1.0f / 60.0f);
        return true;
    }

    void addLegendRow(float x, float y, ccColor4B col, const char* name, const char* action) {
        CCLayerColor* sw = CCLayerColor::create(col, 22.0f, 22.0f);
        sw->ignoreAnchorPointForPosition(false);
        sw->setAnchorPoint(ccp(0.5f, 0.5f));
        sw->setPosition(ccp(x, y));
        m_mainLayer->addChild(sw, 5);
        CCLabelBMFont* n = CCLabelBMFont::create(name, "chatFont.fnt");
        n->setScale(0.55f);
        n->setPosition(ccp(x, y - 18.0f));
        m_mainLayer->addChild(n, 5);
        CCLabelBMFont* a = CCLabelBMFont::create(action, "chatFont.fnt");
        a->setScale(0.45f);
        a->setOpacity(180);
        a->setPosition(ccp(x, y - 30.0f));
        m_mainLayer->addChild(a, 5);
    }

    void addCtrlLine(float x, float y, const char* keys, const char* action) {
        CCLabelBMFont* k = CCLabelBMFont::create(keys, "chatFont.fnt");
        k->setScale(0.55f);
        k->setPosition(ccp(x, y));
        m_mainLayer->addChild(k, 5);
        CCLabelBMFont* a = CCLabelBMFont::create(action, "chatFont.fnt");
        a->setScale(0.45f);
        a->setOpacity(180);
        a->setPosition(ccp(x, y - 14.0f));
        m_mainLayer->addChild(a, 5);
    }

    void doJump() {
        if (jumpT > 0.0f) return;
        if (slideT > 0.0f) {
            // slide-cancel into jump
            slideT = 0.0f;
        }
        jumpT = jumpDur;
    }
    void doSlide() {
        if (slideT > 0.0f) return;
        if (jumpT > 0.0f) {
            // air-slam
            jumpT = 0.0f;
        }
        slideT = slideDur;
    }

    void keyDown(enumKeyCodes key, double p1) override {
        if (!alive) {
            if (key == KEY_Space || key == KEY_Enter) { resetGame(); return; }
            if (key == KEY_Escape) { Popup::keyDown(key, p1); return; }
            return;
        }
        if (key == KEY_Left || key == KEY_A) {
            if (lane > 0) { lane--; targetX = laneX(lane); }
        } else if (key == KEY_Right || key == KEY_D) {
            if (lane < kLanes - 1) { lane++; targetX = laneX(lane); }
        } else if (key == KEY_Up || key == KEY_W || key == KEY_Space) {
            doJump();
        } else if (key == KEY_Down || key == KEY_S) {
            doSlide();
        } else if (key == KEY_Escape) {
            Popup::keyDown(key, p1);
        }
    }
    void spawnObstacle() {
        int l = std::rand() % kLanes;
        int r = std::rand() % 100;
        int kind = 1;
        if (r < 35) kind = 2;
        else if (r < 65) kind = 3;
        else kind = 1;
        float bw = (kPlayW / (float)kLanes) - 8.0f;
        float bh = 38.0f;
        ccColor4B col = ccc4(70, 160, 230, 255);
        if (kind == 2) { bh = 18.0f; col = ccc4(240, 200, 60, 255); }
        else if (kind == 3) { bh = 16.0f; col = ccc4(220, 90, 220, 255); }
        else { bh = 38.0f; col = ccc4(70, 160, 230, 255); }
        CCLayerColor* box = CCLayerColor::create(col, bw, bh);
        box->ignoreAnchorPointForPosition(false);
        box->setAnchorPoint(ccp(0.5f, 0.5f));
        box->setTag(kind);
        float cx = laneX(l);
        box->setPosition(ccp(cx, kPlayH + bh));
        track->addChild(box, 5);
        if (kind == 3) {
            CCLayerColor* bar = CCLayerColor::create(ccc4(150, 50, 150, 255), bw + 6.0f, 5.0f);
            bar->ignoreAnchorPointForPosition(false);
            bar->setAnchorPoint(ccp(0.5f, 0.5f));
            bar->setPosition(ccp(bw * 0.5f, bh + 14.0f));
            box->addChild(bar);
        }
        obs->addObject(box);
    }

    bool hits(CCNode* a, CCNode* b) {
        CCRect ra = a->boundingBox();
        CCRect rb = b->boundingBox();
        ra.origin.x += 4.0f; ra.origin.y += 4.0f;
        ra.size.width -= 8.0f; ra.size.height -= 8.0f;
        return ra.intersectsRect(rb);
    }

    void tick(float dt) {
        if (!alive) {
            // consume swipes so they dont leak
            if (g_swipe == 3) { resetGame(); }
            g_swipe = 0;
            return;
        }

        // consume swipe input
        int sw = g_swipe;
        g_swipe = 0;
        if (sw == 1) {
            if (lane > 0) { lane--; targetX = laneX(lane); }
        } else if (sw == 2) {
            if (lane < kLanes - 1) { lane++; targetX = laneX(lane); }
        } else if (sw == 3) {
            doJump();
        } else if (sw == 4) {
            doSlide();
        }

        // jump/slide timers + visuals
        float yOff = 0.0f;
        float scaleY = 1.0f;
        if (jumpT > 0.0f) {
            jumpT -= dt;
            if (jumpT < 0.0f) jumpT = 0.0f;
            float prog = 1.0f - (jumpT / jumpDur); // 0..1
            yOff = std::sin(prog * 3.14159265f) * 70.0f;
        }
        if (slideT > 0.0f) {
            slideT -= dt;
            if (slideT < 0.0f) slideT = 0.0f;
            scaleY = 0.5f;
            yOff = -8.0f;
        }
        bool jumping = jumpT > 0.0f;
        bool sliding = slideT > 0.0f;

        CCPoint pp = shitPlayer->getPosition();
        float dx = targetX - pp.x;
        float t = dt * 22.0f;
        if (t > 1.0f) t = 1.0f;
        pp.x += dx * t;
        pp.y = 48.0f + yOff;
        shitPlayer->setPosition(pp);
        shitPlayer->setRotation(jumping ? 0.0f : (-dx * 0.6f));
        shitPlayer->setScaleY(0.65f * scaleY);

        scrollOff += spd * dt;
        for (int i = 0; i < 8; i++) {
            CCNode* d = track->getChildByTag(7000 + i);
            if (!d) continue;
            float base = (kPlayH / 8.0f) * (float)i + 9.0f;
            float y = base - std::fmod(scrollOff, kPlayH / 8.0f);
            if (y < 0.0f) y += kPlayH / 8.0f;
            d->setPositionY(y);
        }

        for (int i = (int)obs->count() - 1; i >= 0; i--) {
            CCNode* o = (CCNode*)obs->objectAtIndex(i);
            CCPoint op = o->getPosition();
            op.y -= spd * dt;
            o->setPosition(op);
            if (op.y < -60.0f) {
                o->removeFromParent();
                obs->removeObjectAtIndex(i);
                continue;
            }
            int kind = o->getTag();
            if (kind == 2 && jumping) continue;
            if (kind == 3 && sliding) continue;
            if (hits(shitPlayer, o)) { gameOver(); return; }
        }

        spawnT += dt;
        if (spawnT >= spawnIv) { spawnT = 0.0f; spawnObstacle(); }

        spd += dt * 7.5f;
        if (spawnIv > 0.42f) spawnIv -= dt * 0.025f;

        score += spd * dt * 0.05f;
        scoreLbl->setString(std::to_string((int)score).c_str());
    }

    void gameOver() {
        alive = false;
        shitPlayer->runAction(CCTintTo::create(0.15f, 80, 80, 80));
        overLbl = CCLabelBMFont::create("CRASHED\npress SPACE", "bigFont.fnt");
        overLbl->setAlignment(kCCTextAlignmentCenter);
        overLbl->setScale(0.7f);
        overLbl->setPosition(ccp(kPlayW * 0.5f, kPlayH * 0.55f));
        track->addChild(overLbl, 50);
    }

    void resetGame() {
        if (overLbl) { overLbl->removeFromParent(); overLbl = nullptr; }
        for (int i = (int)obs->count() - 1; i >= 0; i--) {
            ((CCNode*)obs->objectAtIndex(i))->removeFromParent();
        }
        obs->removeAllObjects();
        shitPlayer->stopAllActions();
        shitPlayer->setColor(ccc3(255, 255, 255));
        GameManager* gm = GameManager::sharedState();
        shitPlayer->setColor(gm->colorForIdx(gm->getPlayerColor()));
        shitPlayer->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
        shitPlayer->updateColors();
        shitPlayer->setScaleY(0.65f);
        lane = 1;
        targetX = laneX(1);
        shitPlayer->setPosition(ccp(targetX, 48.0f));
        shitPlayer->setRotation(0.0f);
        jumpT = 0.0f;
        slideT = 0.0f;
        spd = 140.0f;
        spawnIv = 1.05f;
        spawnT = 0.0f;
        score = 0.0f;
        alive = true;
    }

    void onClose(CCObject* o) override {
        this->unschedule(schedule_selector(SubwayPopup::tick));
        if (obs) { obs->release(); obs = nullptr; }
        Popup::onClose(o);
    }

public:
    static SubwayPopup* create() {
        SubwayPopup* p = new SubwayPopup();
        if (p && p->init(420.0f, 380.0f)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
};

class $modify(SubwayMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        std::string logoPath = geode::utils::string::pathToString(
            Mod::get()->getResourcesDir() / "logo.png"
        );
        CCSprite* spr = CCSprite::create(logoPath.c_str());
        if (!spr) spr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        spr->setScale(1.0f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SubwayMenuLayer::onSubway));
        btn->setID("subway-btn"_spr);
        CCNode* menu = this->getChildByID("bottom-menu");
        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }
    void onSubway(CCObject*) {
        SubwayPopup* p = SubwayPopup::create();
        if (p) p->show();
    }
};