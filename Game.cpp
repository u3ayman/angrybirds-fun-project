#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace {
constexpr float ppm = 30, W = 1280, H = 720, gH = 40, bR = 18, pR = 20, bW = 48, bH = 24, sX = 220, sY = 520, pullM = 140, lStr = 32, bDmg = 9999.f, pHp = 200, wUp = .75f, bWk2 = .04f, st2 = .0025f;
constexpr float sHgt = 120, sYf = .22f, vsd = .55f, fjd = .4f;
constexpr int sStep = 1000;
const char *pathB[] = {"circle-vector-flag-iran-banner-circle-vector-flag-iran-banner-national-symbol-middle-east-country-iranian-flag-badge-221157105.png", "circle-vector-flag.png"},
             *pathP[] = {"images.png"}, *pathG[] = {"Backgournd.jpg", "Background.jpg"}, *pathS[] = {"R.png"}, *pathF[] = {"arial.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf"};
#define ZN(a) (sizeof(a) / sizeof((a)[0]))
bool ldT(sf::Texture& t, const char** p, size_t n) { for (size_t i = 0; i < n; i++) if (t.loadFromFile(p[i])) return true; return false; }
void sol(sf::Texture& t, sf::Color c) { sf::Image i; i.resize({128, 128}, c); void(t.loadFromImage(i)); }
void sCirc(sf::Sprite& s, const sf::Texture& t, float r) {
    s.setTexture(t, 1);
    auto z = t.getSize();
    float u = (float)std::max(1u, z.x), v = (float)std::max(1u, z.y), d = 2 * r, sc = d / std::max(u, v);
    s.setScale({sc, sc});
    s.setOrigin({u * .5f, v * .5f});
}
sf::Vector2f px(b2Vec2 m) { return {m.x * ppm, m.y * ppm}; }
b2Vec2 me(sf::Vector2f p) { return {p.x / ppm, p.y / ppm}; }
float len(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
sf::Vector2f nrm(sf::Vector2f v) { float L = len(v); return L < 1e-4f ? sf::Vector2f{} : sf::Vector2f{v.x / L, v.y / L}; }
sf::Vector2f clp(sf::Vector2f o, sf::Vector2f d) { sf::Vector2f D = d - o; float L = len(D); return L <= pullM ? d : o + nrm(D) * pullM; }
struct Bird { b2BodyId id = b2_nullBodyId; sf::Sprite sp; bool shot = 0, drag = 0; sf::Vector2f dpx; };
struct Blk { b2BodyId id = b2_nullBodyId; sf::RectangleShape sh; };
enum class Mat : std::uint8_t { Ice, Wood, Stone };
enum class Kd : std::uint8_t { X, Bird, Pig, Blok };
struct Tag { Kd k = Kd::X; Mat m = Mat::Ice; int ix = -1; };
struct Pig { b2BodyId id = b2_nullBodyId; sf::Sprite sp; bool live = 1; float hp = 100; };
struct BS { sf::Vector2f c, s; Mat m = Mat::Wood; };
struct Lvl { std::vector<sf::Vector2f> pigs; std::vector<BS> blks; };
sf::Color mc(Mat t) { return t == Mat::Stone ? sf::Color(150, 150, 160) : t == Mat::Wood ? sf::Color(160, 110, 60) : sf::Color(170, 220, 255); }
float mdm(Mat t) { return t == Mat::Stone ? 3.f : t == Mat::Wood ? 2.f : 1.f; }
b2BodyId stBox(b2WorldId w, sf::Vector2f c, sf::Vector2f s) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_staticBody;
    bd.position = me(c);
    bd.rotation = b2MakeRot(0);
    b2BodyId id = b2CreateBody(w, &bd);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.material.friction = .8f;
    sd.material.restitution = .1f;
    sd.density = 0;
    const b2Polygon bx = b2MakeBox(s.x * .5f / ppm, s.y * .5f / ppm);
    b2CreatePolygonShape(id, &sd, &bx);
    return id;
}
Bird mkBird(b2WorldId w, Tag* tg, const sf::Texture& tx) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = me({sX, sY});
    bd.linearDamping = .15f;
    bd.angularDamping = .4f;
    bd.userData = tg;
    b2BodyId id = b2CreateBody(w, &bd);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 3.5f;
    sd.material.friction = .5f;
    sd.material.restitution = .2f;
    sd.enableHitEvents = true;
    b2Circle c{};
    c.radius = bR / ppm;
    b2CreateCircleShape(id, &sd, &c);
    Bird r{id, sf::Sprite(tx), 0, 0, {sX, sY}};
    sCirc(r.sp, tx, bR);
    return r;
}
Blk mkBlk(b2WorldId w, sf::Vector2f c, sf::Vector2f s, Mat m, Tag* tg, b2BodyType bt) {
    Blk b;
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = bt;
    bd.position = me(c);
    bd.linearDamping = .05f;
    bd.angularDamping = .2f;
    bd.userData = tg;
    b.id = b2CreateBody(w, &bd);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = .8f;
    sd.material.friction = .7f;
    sd.material.restitution = .05f;
    sd.enableHitEvents = true;
    const b2Polygon bx = b2MakeBox(s.x * .5f / ppm, s.y * .5f / ppm);
    b2CreatePolygonShape(b.id, &sd, &bx);
    b.sh = sf::RectangleShape(s);
    b.sh.setOrigin({s.x * .5f, s.y * .5f});
    b.sh.setFillColor(mc(m));
    return b;
}
Pig mkPig(b2WorldId w, sf::Vector2f c, Tag* tg, b2BodyType bt, const sf::Texture& tx) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = bt;
    bd.position = me(c);
    bd.linearDamping = .25f;
    bd.angularDamping = .35f;
    bd.userData = tg;
    b2BodyId id = b2CreateBody(w, &bd);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.2f;
    sd.material.friction = .6f;
    sd.material.restitution = .15f;
    sd.enableHitEvents = true;
    b2Circle ci{};
    ci.radius = pR / ppm;
    b2CreateCircleShape(id, &sd, &ci);
    Pig p{id, sf::Sprite(tx), 1, pHp};
    sCirc(p.sp, tx, pR);
    return p;
}
void synS(sf::Shape& s, b2BodyId id) {
    s.setPosition(px(b2Body_GetPosition(id)));
    b2Rot r = b2Body_GetRotation(id);
    s.setRotation(sf::radians(std::atan2(r.s, r.c)));
}
void synG(sf::Sprite& s, b2BodyId id) {
    s.setPosition(px(b2Body_GetPosition(id)));
    b2Rot r = b2Body_GetRotation(id);
    s.setRotation(sf::radians(std::atan2(r.s, r.c)));
}
bool ov(b2BodyId a, float ar, b2BodyId b, float br) {
    sf::Vector2f d = px(b2Body_GetPosition(a)) - px(b2Body_GetPosition(b));
    float R = ar + br;
    return d.x * d.x + d.y * d.y <= R * R;
}
void des(b2BodyId& id) {
    if (B2_IS_NON_NULL(id)) b2DestroyBody(id), id = b2_nullBodyId;
}
bool anyP(const std::vector<Pig>& v) { for (auto& p : v) if (p.live) return 1; return 0; }
struct Rate { int st; const char* msg; };
Rate rate(int u) { return u <= 1 ? Rate{3, "Gamid"} : u <= 3 ? Rate{2, "3ash"} : Rate{1, "ygy mnk"}; }
bool ldF(sf::Font& f) { for (auto p : pathF) if (f.openFromFile(p)) return 1; return 0; }
void cTx(sf::Text& t, float cx, float y) {
    auto b = t.getLocalBounds();
    t.setOrigin({b.position.x + b.size.x * .5f, b.position.y + b.size.y * .5f});
    t.setPosition({cx, y});
}
void drO(sf::RenderWindow& w, const sf::Font* F, bool win, int nst, const std::string& vm, int scg, int tot, const char* failMsg) {
    sf::RectangleShape sh({W, H});
    sh.setFillColor(win ? sf::Color(0, 0, 0, 170) : sf::Color(40, 0, 0, 190));
    w.draw(sh);
    for (int i = 0; i < 3; i++) {
        sf::CircleShape s(16);
        s.setOrigin({16, 16});
        s.setPosition({W * .5f + (i - 1) * 52.f, 185});
        s.setFillColor(win ? (i < nst ? sf::Color(255, 220, 60) : sf::Color(70, 70, 75)) : sf::Color(55, 55, 60));
        w.draw(s);
    }
    if (!F) return;
    sf::Text h(*F, win ? "Level complete!" : "Out of birds!", 42u);
    h.setFillColor(win ? sf::Color::White : sf::Color(255, 200, 200));
    cTx(h, W * .5f, 280);
    w.draw(h);
    sf::Text m(*F, "", 36u);
    m.setString(win ? vm : failMsg);
    m.setFillColor({255, 230, 150});
    cTx(m, W * .5f, 345);
    w.draw(m);
    if (win) {
        sf::Text x(*F, "This level: +" + std::to_string(scg) + "     Total: " + std::to_string(tot), 26u);
        x.setFillColor({220, 220, 230});
        cTx(x, W * .5f, 410);
        w.draw(x);
    }
    sf::Text hi(*F, win ? "Press Space for next level" : "Press Space to retry", 22u);
    hi.setFillColor(win ? sf::Color(180, 200, 255) : sf::Color(255, 180, 180));
    cTx(hi, W * .5f, 500);
    w.draw(hi);
}
} // namespace
int main() {
    sf::RenderWindow win(sf::VideoMode(sf::Vector2u((unsigned)W, (unsigned)H)), "Angry Birds Prototype");
    win.setFramerateLimit(60);
    sf::SoundBuffer bL, bP, bI;
    std::optional<sf::Sound> sL, sP, sI;
    if (bL.loadFromFile("weew.mp3")) sL.emplace(bL);
    if (bP.loadFromFile("ooh.mp3")) sP.emplace(bP);
    if (bI.loadFromFile("intro.mp3")) sI.emplace(bI);
    sf::Music mEnd;
    auto stS = [&] {
        if (sL) sL->stop();
        if (sP) sP->stop();
        if (sI) sI->stop();
    };
    auto ply = [&](const char* f) {
        mEnd.stop();
        stS();
        return mEnd.openFromFile(f) ? (mEnd.setVolume(100), mEnd.play(), 1) : 0;
    };
    sf::Texture tB, tP, tG, tS;
    if (!ldT(tB, pathB, ZN(pathB))) sol(tB, {220, 50, 50});
    if (!ldT(tP, pathP, ZN(pathP))) sol(tP, {70, 200, 70});
    if (!ldT(tG, pathG, ZN(pathG))) sol(tG, {150, 200, 255});
    if (!ldT(tS, pathS, ZN(pathS))) sol(tS, {120, 80, 40});
    sf::Sprite gSpr(tG), slSpr(tS);
    if (auto z = tG.getSize(); z.x && z.y) gSpr.setScale({W / (float)z.x, H / (float)z.y});
    if (auto z = tS.getSize(); z.x && z.y) slSpr.setScale({sHgt / z.y, sHgt / z.y}), slSpr.setOrigin({z.x * .5f, z.y * sYf}), slSpr.setPosition({sX, sY});
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = {0, 9.8f};
    wd.hitEventThreshold = .25f;
    b2WorldId Wd = b2CreateWorld(&wd);
    stBox(Wd, {W * .5f, H - gH * .5f}, {W, gH});
    stBox(Wd, {40, H * .5f}, {80, H});
    stBox(Wd, {W - 40, H * .5f}, {80, H});
    stBox(Wd, {W * .5f, 0}, {W, 80});
    const std::vector<Lvl> L = {
        {.pigs = {{980, 520}}, .blks = {{{900, 560}, {bW, bH}}, {{950, 560}, {bW, bH}}, {{1000, 560}, {bW, bH}}, {{900, 534}, {bW, bH}}, {{950, 534}, {bW, bH}}, {{1000, 534}, {bW, bH}}, {{900, 508}, {bW, bH}, Mat::Stone}, {{950, 508}, {bW, bH}, Mat::Stone}, {{1000, 508}, {bW, bH}, Mat::Stone}}},
        {.pigs = {{1040, 480}}, .blks = {{{980, 570}, {bW, 120}, Mat::Stone}, {{1100, 570}, {bW, 120}, Mat::Stone}, {{1040, 520}, {180, bH}}, {{1040, 470}, {120, bH}}}},
        {.pigs = {{1120, 560}}, .blks = {{{980, 580}, {260, bH}, Mat::Stone}, {{980, 550}, {260, bH}, Mat::Ice}, {{980, 520}, {260, bH}}, {{980, 490}, {260, bH}, Mat::Stone}}},
        {.pigs = {{980, 560}, {1100, 560}}, .blks = {{{980, 600}, {220, bH}, Mat::Stone}, {{1100, 600}, {220, bH}, Mat::Stone}, {{1040, 540}, {320, bH}}, {{1040, 510}, {320, bH}, Mat::Ice}}},
        {.pigs = {{940, 560}, {1040, 560}, {1140, 560}}, .blks = {{{940, 600}, {120, bH}, Mat::Stone}, {{1040, 600}, {120, bH}, Mat::Stone}, {{1140, 600}, {120, bH}, Mat::Stone}, {{990, 540}, {220, bH}}, {{1090, 540}, {220, bH}}, {{1040, 510}, {380, bH}, Mat::Ice}}}};
    size_t li = 0;
    std::optional<Bird> B;
    std::vector<Pig> P;
    std::vector<Blk> K;
    std::vector<Tag> T;
    float dmgW = 0, idle = 0;
    const float gY = H - gH;
    constexpr int maxB = 4;
    int br = maxB, bu = 0, tot = 0, scg = 0;
    bool vic = 0, fail = 0, pse = 0, vJ = 0, fJ = 0;
    float vT = -1, fT = -1;
    int vst = 0;
    std::string vmsg;
    sf::Font fU;
    bool fOk = ldF(fU);
    auto stb = [&] {
        constexpr float dt = 1 / 60.f;
        int ss = 0;
        for (int s = 0; s < 240; s++) {
            b2World_Step(Wd, dt, 4);
            bool sl = 1;
            for (auto& p : P) {
                if (!p.live || B2_IS_NULL(p.id)) continue;
                b2Vec2 v = b2Body_GetLinearVelocity(p.id);
                if (v.x * v.x + v.y * v.y > st2) sl = 0;
            }
            if (sl) for (auto& b : K) {
                    if (B2_IS_NULL(b.id)) continue;
                    b2Vec2 v = b2Body_GetLinearVelocity(b.id);
                    if (v.x * v.x + v.y * v.y > st2) { sl = 0; break; }
                }
            ss = sl ? ss + 1 : 0;
            if (ss >= 45) break;
        }
        b2Vec2 z{};
        for (auto& p : P) if (B2_IS_NON_NULL(p.id)) b2Body_SetLinearVelocity(p.id, z), b2Body_SetAngularVelocity(p.id, 0), b2Body_SetAwake(p.id, false);
        for (auto& b : K) if (B2_IS_NON_NULL(b.id)) b2Body_SetLinearVelocity(b.id, z), b2Body_SetAngularVelocity(b.id, 0), b2Body_SetAwake(b.id, false);
    };
    auto load = [&](size_t ix) {
        vic = fail = vJ = fJ = 0, vst = 0, vmsg.clear(), scg = 0, bu = 0, vT = fT = -1, mEnd.stop();
        if (B) des(B->id);
        for (auto& p : P) des(p.id);
        for (auto& b : K) des(b.id);
        P.clear(), K.clear(), T.clear(), br = maxB, idle = 0;
        T.reserve(1 + L[ix].pigs.size() + L[ix].blks.size());
        T.push_back({Kd::Bird, Mat::Stone, -1});
        B.emplace(mkBird(Wd, &T[0], tB));
        for (size_t i = 0; i < L[ix].pigs.size(); i++) {
            sf::Vector2f c = L[ix].pigs[i];
            c.y = std::max(c.y, gY - pR);
            T.push_back({Kd::Pig, Mat::Ice, (int)P.size()});
            P.push_back(mkPig(Wd, c, &T.back(), b2_dynamicBody, tP));
        }
        for (auto& s : L[ix].blks) {
            sf::Vector2f c = s.c;
            c.y = std::max(c.y, gY - s.s.y * .5f);
            T.push_back(Tag{Kd::Blok, s.m, -1});
            K.push_back(mkBlk(Wd, c, s.s, s.m, &T.back(), b2_dynamicBody));
        }
        dmgW = wUp;
        stb();
    };
    load(li);
    if (sI) sI->play();
    sf::RectangleShape gr({W, gH});
    gr.setPosition({0, H - gH});
    gr.setFillColor({80, 140, 80});
    sf::VertexArray ln(sf::PrimitiveType::Lines, 2);
    ln[0].color = ln[1].color = {40, 40, 40, 180};
    sf::Clock clk;
    bool frz = 0;
    auto ttl = [&] {
        if (vic) return win.setTitle("Clear " + std::to_string(vst) + "* Score " + std::to_string(tot) + " [Space] next"), void();
        if (fail) return win.setTitle("No birds 0* [Space] retry"), void();
        if (pse) return win.setTitle("Paused [Esc][R][Q]"), void();
        int ap = 0;
        for (auto& p : P) if (p.live) ap++;
        win.setTitle("L" + std::to_string((int)li + 1) + " P" + std::to_string(ap) + " B" + std::to_string(br) + " S" + std::to_string(tot));
    };
    ttl();
    while (win.isOpen()) {
        while (auto e = win.pollEvent()) {
            if (e->is<sf::Event::Closed>()) win.close();
            else if (auto k = e->getIf<sf::Event::KeyPressed>()) {
                if (fail && (k->code == sf::Keyboard::Key::Space || k->code == sf::Keyboard::Key::Enter)) load(li), pse = 0, ttl();
                else if (vic && (k->code == sf::Keyboard::Key::Space || k->code == sf::Keyboard::Key::Enter)) li = (li + 1) % L.size(), load(li), pse = 0, ttl();
                else if (k->code == sf::Keyboard::Key::Escape && !vic && !fail) pse = !pse, ttl();
                else if (pse && k->code == sf::Keyboard::Key::R) load(li), pse = 0, ttl();
                else if (pse && k->code == sf::Keyboard::Key::Q) win.close();
            } else if (pse || vic || fail) continue;
            else if (const auto* pb = e->getIf<sf::Event::MouseButtonPressed>()) {
                if (B && !B->shot && pb->button == sf::Mouse::Button::Left) {
                    sf::Vector2f m((float)pb->position.x, (float)pb->position.y);
                    if (len(m - px(b2Body_GetPosition(B->id))) <= bR * 1.5f) B->drag = 1;
                }
            } else if (const auto* mm = e->getIf<sf::Event::MouseMoved>()) {
                if (B && B->drag) {
                    sf::Vector2f mp((float)mm->position.x, (float)mm->position.y);
                    B->dpx = clp({sX, sY}, mp);
                    b2Body_SetTransform(B->id, me(B->dpx), b2MakeRot(0));
                    b2Body_SetLinearVelocity(B->id, b2Vec2{0, 0});
                    b2Body_SetAngularVelocity(B->id, 0);
                    b2Body_SetAwake(B->id, true);
                }
            } else if (const auto* pr = e->getIf<sf::Event::MouseButtonReleased>()) {
                if (B && B->drag && pr->button == sf::Mouse::Button::Left) {
                    B->drag = 0, B->shot = 1, bu++, br = std::max(0, br - 1), idle = 0, ttl();
                    b2Vec2 im = me(sf::Vector2f{sX, sY} - B->dpx);
                    im.x *= lStr, im.y *= lStr;
                    b2Body_SetAwake(B->id, true);
                    b2Body_ApplyLinearImpulseToCenter(B->id, im, true);
                    if (sL) sL->play();
                }
            }
        }
        float dt = clk.restart().asSeconds();
        frz = pse || vic || fail;
        if (vic && !vJ) {
            vT -= dt;
            if (vT <= 0) ply("stars.mp3"), vJ = 1;
        }
        if (fail && !fJ) {
            fT -= dt;
            if (fT <= 0) ply("wawawawa.mp3"), fJ = 1;
        }
        if (!frz) {
            dmgW = std::max(0.f, dmgW - dt);
            b2World_Step(Wd, dt, 4);
            auto ev = b2World_GetContactEvents(Wd);
            for (int i = 0; i < ev.hitCount; i++) {
                auto& h = ev.hitEvents[i];
                if (!b2Shape_IsValid(h.shapeIdA) || !b2Shape_IsValid(h.shapeIdB)) continue;
                b2BodyId a = b2Shape_GetBody(h.shapeIdA), b = b2Shape_GetBody(h.shapeIdB);
                auto* ta = (Tag*)b2Body_GetUserData(a), *tb = (Tag*)b2Body_GetUserData(b);
                if (!ta || !tb) continue;
                bool pa = ta->k == Kd::Pig, pb = tb->k == Kd::Pig;
                if (!pa && !pb) continue;
                const Tag &pg = pa ? *ta : *tb, &ot = pa ? *tb : *ta;
                if (pg.ix < 0 || pg.ix >= (int)P.size()) continue;
                Pig& pig = P[(size_t)pg.ix];
                if (!pig.live) continue;
                float dm = ot.k == Kd::Bird ? bDmg : (ot.k == Kd::Blok && dmgW <= 0 ? h.approachSpeed * 18.f * mdm(ot.m) : 0);
                if (dm > 0 && (pig.hp -= dm) <= 0) pig.hp = 0, pig.live = 0, sP ? sP->play() : void(), ttl();
            }
        }
        if (!frz && B && B->shot) for (auto& p : P) if (p.live && ov(B->id, bR, p.id, pR)) p.live = 0, p.hp = 0, sP ? sP->play() : void(), ttl();
        if (!frz && !fail && !vic && !anyP(P)) {
            Rate r = rate(bu);
            vst = r.st, vmsg = r.msg, scg = vst * sStep, tot += scg, vic = 1, pse = 0, vJ = 0, vT = vsd, ttl();
        }
        if (!frz && B && B->shot && B2_IS_NON_NULL(B->id)) {
            sf::Vector2f bp = px(b2Body_GetPosition(B->id));
            bool oob = bp.x < -200 || bp.x > W + 200 || bp.y < -200 || bp.y > H + 200;
            b2Vec2 v = b2Body_GetLinearVelocity(B->id);
            idle = (v.x * v.x + v.y * v.y < bWk2) ? idle + dt : 0;
            if (oob || idle > 1.25f) {
                des(B->id), B.reset();
                if (br > 0) B.emplace(mkBird(Wd, &T[0], tB)), ttl();
                else if (anyP(P)) fail = 1, pse = 0, fJ = 0, fT = fjd, ttl();
                else load(li), ttl();
            }
        }
        if (B) {
            if (B->drag) B->sp.setPosition(B->dpx);
            else synG(B->sp, B->id);
        }
        for (auto& p : P) if (p.live) synG(p.sp, p.id);
        for (auto& b : K) synS(b.sh, b.id);
        win.clear({150, 200, 255});
        win.draw(gSpr);
        win.draw(gr);
        win.draw(slSpr);
        if (B && B->drag) {
            ln[0].position = {sX, sY}, ln[1].position = B->dpx, win.draw(ln);
            b2Vec2 im = me(sf::Vector2f{sX, sY} - B->dpx), p0 = me(B->dpx);
            im.x *= lStr, im.y *= lStr;
            float mass = b2Body_GetMass(B->id);
            b2Vec2 v0 = mass > 1e-4f ? b2Vec2{im.x / mass, im.y / mass} : b2Vec2{}, g = b2World_GetGravity(Wd);
            for (int i = 1; i <= 20; i++) {
                float t = i * .08f;
                sf::CircleShape d(3);
                d.setOrigin({3, 3});
                d.setPosition(px({p0.x + v0.x * t + .5f * g.x * t * t, p0.y + v0.y * t + .5f * g.y * t * t}));
                d.setFillColor({40, 40, 40, 140});
                win.draw(d);
            }
        }
        if (B) win.draw(B->sp);
        for (auto& p : P) if (p.live) win.draw(p.sp);
        for (auto& b : K) win.draw(b.sh);
        if (vic) drO(win, fOk ? &fU : nullptr, 1, vst, vmsg, scg, tot, "");
        if (fail) drO(win, fOk ? &fU : nullptr, 0, 0, "", 0, 0, "7sl 5eir");
        if (fOk) {
            sf::Text lh(fU, "Level " + std::to_string((int)li + 1), 28u);
            lh.setFillColor(sf::Color::White);
            lh.setOutlineThickness(2);
            lh.setOutlineColor({0, 0, 0, 200});
            auto lb = lh.getLocalBounds();
            lh.setOrigin({lb.position.x + lb.size.x, lb.position.y});
            lh.setPosition({W - 18, 16});
            win.draw(lh);
        }
        win.display();
    }
    b2DestroyWorld(Wd);
    return 0;
}