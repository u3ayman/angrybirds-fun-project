#include <SFML/Audio.hpp> // SFML: sounds, music, buffers
#include <SFML/Graphics.hpp> // SFML: window, textures, sprites, shapes, text, draw
#include <box2d/box2d.h> // Box2D 3: 2D rigid-body physics API
#include <algorithm> // std::max and similar algorithms
#include <cmath> // std::sqrt for vector length
#include <cstdint> // fixed-width integers (used by enums)
#include <optional> // std::optional for bird and sounds without default constructors
#include <string> // std::string for window title and victory message
#include <vector> // std::vector for pigs, blocks, tags, levels
namespace { // anonymous namespace: internal linkage for helpers below
constexpr float ppm = 30, W = 1280, H = 720, gH = 40, bR = 18, pR = 20, bW = 48, bH = 24, sX = 220, sY = 520, pullM = 140, lStr = 32, bDmg = 9999.f, pHp = 200, wUp = .75f, bWk2 = .04f, st2 = .0025f; // gameplay/layout constants (px, m/s^2 thresholds)
constexpr float sHgt = 120, sYf = .22f, vsd = .55f, fjd = .4f; // slingshot sprite height, origin Y fraction, jingle delays (seconds)
constexpr int sStep = 1000; // score points added per star tier (stars * sStep)
const char *pathB[] = {"circle-vector-flag-iran-banner-circle-vector-flag-iran-banner-national-symbol-middle-east-country-iranian-flag-badge-221157105.png", "circle-vector-flag.png"}, // bird texture filenames to try in order
             *pathP[] = {"images.png"}, *pathG[] = {"Backgournd.jpg", "Background.jpg"}, *pathS[] = {"R.png"}, *pathF[] = {"arial.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf"}; // pig, bg, sling, UI font paths
#define ZN(a) (sizeof(a) / sizeof((a)[0])) // element count of a C array (for texture/font path lists)
bool ldT(sf::Texture& t, const char** p, size_t n) { for (size_t i = 0; i < n; i++) if (t.loadFromFile(p[i])) return true; return false; } // load first existing image file from list p[0..n)
void sol(sf::Texture& t, sf::Color c) { sf::Image i; i.resize({128, 128}, c); void(t.loadFromImage(i)); } // fallback: fill texture with solid color image
void sCirc(sf::Sprite& s, const sf::Texture& t, float r) { // scale sprite so its larger side matches circle diameter 2*r; center origin for physics sync
    s.setTexture(t, 1); // bind texture to sprite (true = reset rect to full texture)
    auto z = t.getSize(); // pixel width/height of texture
    float u = (float)std::max(1u, z.x), v = (float)std::max(1u, z.y), d = 2 * r, sc = d / std::max(u, v); // uniform scale to fit diameter d in pixels
    s.setScale({sc, sc}); // apply same scale on X and Y
    s.setOrigin({u * .5f, v * .5f}); // origin at visual center of sprite
} // end sCirc
sf::Vector2f px(b2Vec2 m) { return {m.x * ppm, m.y * ppm}; } // meters to screen pixels using ppm
b2Vec2 me(sf::Vector2f p) { return {p.x / ppm, p.y / ppm}; } // pixels to meters for Box2D
float len(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); } // Euclidean length of a 2D vector
sf::Vector2f nrm(sf::Vector2f v) { float L = len(v); return L < 1e-4f ? sf::Vector2f{} : sf::Vector2f{v.x / L, v.y / L}; } // unit vector, or zero if degenerate
sf::Vector2f clp(sf::Vector2f o, sf::Vector2f d) { sf::Vector2f D = d - o; float L = len(D); return L <= pullM ? d : o + nrm(D) * pullM; } // clamp drag point to max distance pullM from slingshot anchor o
struct Bird { b2BodyId id = b2_nullBodyId; sf::Sprite sp; bool shot = 0, drag = 0; sf::Vector2f dpx; }; // player projectile: physics id, drawable, launched/dragging, aim position while dragging
struct Blk { b2BodyId id = b2_nullBodyId; sf::RectangleShape sh; }; // tower block: body + axis-aligned rectangle for drawing
enum class Mat : std::uint8_t { Ice, Wood, Stone }; // block material (affects color and damage multiplier)
enum class Kd : std::uint8_t { X, Bird, Pig, Blok }; // body kind stored in Box2D userData for hit routing
struct Tag { Kd k = Kd::X; Mat m = Mat::Ice; int ix = -1; }; // per-body tag: kind, material (blocks), pig index in vector P when kind is Pig
struct Pig { b2BodyId id = b2_nullBodyId; sf::Sprite sp; bool live = 1; float hp = 100; }; // target: physics, sprite, alive flag, health
struct BS { sf::Vector2f c, s; Mat m = Mat::Wood; }; // level block spec: center px, size px, material
struct Lvl { std::vector<sf::Vector2f> pigs; std::vector<BS> blks; }; // one level: pig spawn points and block list
sf::Color mc(Mat t) { return t == Mat::Stone ? sf::Color(150, 150, 160) : t == Mat::Wood ? sf::Color(160, 110, 60) : sf::Color(170, 220, 255); } // RGB fill for block material
float mdm(Mat t) { return t == Mat::Stone ? 3.f : t == Mat::Wood ? 2.f : 1.f; } // scales block→pig crush damage by material hardness
b2BodyId stBox(b2WorldId w, sf::Vector2f c, sf::Vector2f s) { // create immovable axis-aligned box (ground/walls/ceiling)
    b2BodyDef bd = b2DefaultBodyDef(); // stack-allocated Box2D body definition with defaults
    bd.type = b2_staticBody; // infinite mass: does not move from forces
    bd.position = me(c); // world center in meters
    bd.rotation = b2MakeRot(0); // no initial rotation
    b2BodyId id = b2CreateBody(w, &bd); // register body in world w
    b2ShapeDef sd = b2DefaultShapeDef(); // default shape properties
    sd.material.friction = .8f; // resist sliding
    sd.material.restitution = .1f; // slight bounce
    sd.density = 0; // static geometry: no mass from this shape
    const b2Polygon bx = b2MakeBox(s.x * .5f / ppm, s.y * .5f / ppm); // half-extents in meters for box shape
    b2CreatePolygonShape(id, &sd, &bx); // attach box collider to body
    return id; // caller keeps this id to destroy world later (not stored for static borders here)
} // end stBox
Bird mkBird(b2WorldId w, Tag* tg, const sf::Texture& tx) { // spawn bird circle at slingshot with physics + sprite
    b2BodyDef bd = b2DefaultBodyDef(); // dynamic body defaults
    bd.type = b2_dynamicBody; // moves under gravity and impulses
    bd.position = me({sX, sY}); // start at slingshot anchor in meters
    bd.linearDamping = .15f; // air resistance on linear motion
    bd.angularDamping = .4f; // spin decay
    bd.userData = tg; // pointer back to Tag for contact queries
    b2BodyId id = b2CreateBody(w, &bd); // create simulated body
    b2ShapeDef sd = b2DefaultShapeDef(); // circle shape defaults
    sd.density = 3.5f; // mass from area * density
    sd.material.friction = .5f; // slide on surfaces
    sd.material.restitution = .2f; // bounce a bit
    sd.enableHitEvents = true; // allow hit events for pig damage
    b2Circle c{}; // circle shape centered on body origin
    c.radius = bR / ppm; // radius in meters from pixel radius bR
    b2CreateCircleShape(id, &sd, &c); // attach circle fixture
    Bird r{id, sf::Sprite(tx), 0, 0, {sX, sY}}; // pack Bird struct: not shot, not dragging, drag px starts at slingshot
    sCirc(r.sp, tx, bR); // scale sprite to match physics circle
    return r; // return by value to emplace into std::optional<Bird>
} // end mkBird
Blk mkBlk(b2WorldId w, sf::Vector2f c, sf::Vector2f s, Mat m, Tag* tg, b2BodyType bt) { // dynamic rectangular block
    Blk b; // default-constructed block holder
    b2BodyDef bd = b2DefaultBodyDef(); // body definition
    bd.type = bt; // usually dynamic so tower can fall
    bd.position = me(c); // center in meters
    bd.linearDamping = .05f; // light linear drag
    bd.angularDamping = .2f; // light angular drag
    bd.userData = tg; // link to Tag for material in hits
    b.id = b2CreateBody(w, &bd); // create and store id in Blk
    b2ShapeDef sd = b2DefaultShapeDef(); // fixture defaults
    sd.density = .8f; // block mass scale
    sd.material.friction = .7f; // grip
    sd.material.restitution = .05f; // almost inelastic
    sd.enableHitEvents = true; // generate hits vs pigs
    const b2Polygon bx = b2MakeBox(s.x * .5f / ppm, s.y * .5f / ppm); // box half-size in meters
    b2CreatePolygonShape(b.id, &sd, &bx); // attach polygon
    b.sh = sf::RectangleShape(s); // SFML rectangle same pixel size as physics box
    b.sh.setOrigin({s.x * .5f, s.y * .5f}); // origin at center for sync with body position
    b.sh.setFillColor(mc(m)); // color encodes material
    return b; // completed block
} // end mkBlk
Pig mkPig(b2WorldId w, sf::Vector2f c, Tag* tg, b2BodyType bt, const sf::Texture& tx) { // dynamic pig circle + sprite
    b2BodyDef bd = b2DefaultBodyDef(); // body def
    bd.type = bt; // dynamic pig
    bd.position = me(c); // spawn center meters
    bd.linearDamping = .25f; // slightly more drag than bird
    bd.angularDamping = .35f; // spin damping
    bd.userData = tg; // Tag with pig index
    b2BodyId id = b2CreateBody(w, &bd); // create body
    b2ShapeDef sd = b2DefaultShapeDef(); // shape def
    sd.density = 1.2f; // pig mass feel
    sd.material.friction = .6f; // friction with ground/blocks
    sd.material.restitution = .15f; // small bounce
    sd.enableHitEvents = true; // hits for damage
    b2Circle ci{}; // circle fixture
    ci.radius = pR / ppm; // pig radius in meters
    b2CreateCircleShape(id, &sd, &ci); // attach circle
    Pig p{id, sf::Sprite(tx), 1, pHp}; // alive with full HP constant pHp
    sCirc(p.sp, tx, pR); // scale pig sprite to physics radius
    return p; // return pig
} // end mkPig
void synS(sf::Shape& s, b2BodyId id) { // copy Box2D transform to SFML rectangle
    s.setPosition(px(b2Body_GetPosition(id))); // meters → pixels for top-left (with centered origin)
    b2Rot r = b2Body_GetRotation(id); // read rotation as sincos pair
    s.setRotation(sf::radians(std::atan2(r.s, r.c))); // angle = atan2(sin, cos)
} // end synS
void synG(sf::Sprite& s, b2BodyId id) { // same as synS but for sprites (bird/pigs)
    s.setPosition(px(b2Body_GetPosition(id))); // world position to pixels
    b2Rot r = b2Body_GetRotation(id); // rotation state
    s.setRotation(sf::radians(std::atan2(r.s, r.c))); // apply radians to drawable
} // end synG
bool ov(b2BodyId a, float ar, b2BodyId b, float br) { // circle–circle overlap test in pixel space using body centers
    sf::Vector2f d = px(b2Body_GetPosition(a)) - px(b2Body_GetPosition(b)); // pixel offset between centers
    float R = ar + br; // combined radius in pixels
    return d.x * d.x + d.y * d.y <= R * R; // compare squared distance to squared sum radii
} // end ov
void des(b2BodyId& id) { // destroy Box2D body if handle is valid
    if (B2_IS_NON_NULL(id)) b2DestroyBody(id), id = b2_nullBodyId; // free body and null out reference (comma operator chains)
} // end des
bool anyP(const std::vector<Pig>& v) { for (auto& p : v) if (p.live) return 1; return 0; } // true if at least one pig still alive
struct Rate { int st; const char* msg; }; // star count 1..3 and short rating phrase for UI
Rate rate(int u) { return u <= 1 ? Rate{3, "Gamid"} : u <= 3 ? Rate{2, "3ash"} : Rate{1, "ygy mnk"}; } // map birds-used-this-level to stars/message
bool ldF(sf::Font& f) { for (auto p : pathF) if (f.openFromFile(p)) return 1; return 0; } // load first available UI font file
void cTx(sf::Text& t, float cx, float y) { // center text horizontally and vertically around (cx, y)
    auto b = t.getLocalBounds(); // bounding box of glyphs in local coords
    t.setOrigin({b.position.x + b.size.x * .5f, b.position.y + b.size.y * .5f}); // origin at text bbox center
    t.setPosition({cx, y}); // place centered point at (cx,y)
} // end cTx
void drO(sf::RenderWindow& w, const sf::Font* F, bool win, int nst, const std::string& vm, int scg, int tot, const char* failMsg) { // draw victory (win=1) or failure overlay
    sf::RectangleShape sh({W, H}); // fullscreen quad for dimming
    sh.setFillColor(win ? sf::Color(0, 0, 0, 170) : sf::Color(40, 0, 0, 190)); // dark neutral vs dark red tint
    w.draw(sh); // draw dim layer under text
    for (int i = 0; i < 3; i++) { // three star slots
        sf::CircleShape s(16); // star glyph approximated as circle radius 16
        s.setOrigin({16, 16}); // center origin on circle
        s.setPosition({W * .5f + (i - 1) * 52.f, 185}); // horizontal layout centered on screen
        s.setFillColor(win ? (i < nst ? sf::Color(255, 220, 60) : sf::Color(70, 70, 75)) : sf::Color(55, 55, 60)); // gold earned vs gray; fail all gray
        w.draw(s); // submit star primitive
    } // end for stars
    if (!F) return; // skip text if no font loaded
    sf::Text h(*F, win ? "Level complete!" : "Out of birds!", 42u); // main heading string and character size
    h.setFillColor(win ? sf::Color::White : sf::Color(255, 200, 200)); // white vs light red
    cTx(h, W * .5f, 280); // center heading
    w.draw(h); // draw heading
    sf::Text m(*F, "", 36u); // empty text object to set string next
    m.setString(win ? vm : failMsg); // rating phrase or fixed fail phrase
    m.setFillColor({255, 230, 150}); // warm yellow for message
    cTx(m, W * .5f, 345); // center message
    w.draw(m); // draw message
    if (win) { // score line only on victory
        sf::Text x(*F, "This level: +" + std::to_string(scg) + "     Total: " + std::to_string(tot), 26u); // show points gained and running total
        x.setFillColor({220, 220, 230}); // light gray
        cTx(x, W * .5f, 410); // center score line
        w.draw(x); // draw score line
    } // end if win
    sf::Text hi(*F, win ? "Press Space for next level" : "Press Space to retry", 22u); // input hint
    hi.setFillColor(win ? sf::Color(180, 200, 255) : sf::Color(255, 180, 180)); // blue vs pink hint color
    cTx(hi, W * .5f, 500); // center hint near bottom
    w.draw(hi); // draw hint
} // end drO
} // namespace — end of file-local helpers
int main() { // program entry: window, assets, world, game loop, cleanup
    sf::RenderWindow win(sf::VideoMode(sf::Vector2u((unsigned)W, (unsigned)H)), "Angry Birds Prototype"); // create OS window W×H pixels
    win.setFramerateLimit(60); // cap update rate for stable physics timestep feel
    sf::SoundBuffer bL, bP, bI; // decoded audio for launch, pig death, intro (must outlive sf::Sound)
    std::optional<sf::Sound> sL, sP, sI; // optional because sf::Sound has no default constructor in SFML 3
    if (bL.loadFromFile("weew.mp3")) sL.emplace(bL); // load launch SFX if file exists
    if (bP.loadFromFile("ooh.mp3")) sP.emplace(bP); // load pig death SFX if file exists
    if (bI.loadFromFile("intro.mp3")) sI.emplace(bI); // load intro sting if file exists
    sf::Music mEnd; // streamed music for end-of-level MP3s (stars / wawawawa)
    auto stS = [&] { // lambda: stop all short one-shot sounds before streaming music
        if (sL) sL->stop(); // halt launch sound if playing
        if (sP) sP->stop(); // halt pig sound
        if (sI) sI->stop(); // halt intro if still playing
    }; // end stS lambda
    auto ply = [&](const char* f) { // open path f as music, stop SFX, play; returns 1 on success
        mEnd.stop(); // stop previous streamed track
        stS(); // silence one-shot buffers
        return mEnd.openFromFile(f) ? (mEnd.setVolume(100), mEnd.play(), 1) : 0; // comma expr: set vol, play, return 1; else 0
    }; // end ply lambda
    sf::Texture tB, tP, tG, tS; // bird, pig, background, slingshot textures
    if (!ldT(tB, pathB, ZN(pathB))) sol(tB, {220, 50, 50}); // red fallback if bird images missing
    if (!ldT(tP, pathP, ZN(pathP))) sol(tP, {70, 200, 70}); // green fallback if pig image missing
    if (!ldT(tG, pathG, ZN(pathG))) sol(tG, {150, 200, 255}); // sky-blue fallback if JPG missing
    if (!ldT(tS, pathS, ZN(pathS))) sol(tS, {120, 80, 40}); // brown fallback if sling PNG missing
    sf::Sprite gSpr(tG), slSpr(tS); // fullscreen background sprite and slingshot sprite
    if (auto z = tG.getSize(); z.x && z.y) gSpr.setScale({W / (float)z.x, H / (float)z.y}); // stretch background to cover window
    if (auto z = tS.getSize(); z.x && z.y) slSpr.setScale({sHgt / z.y, sHgt / z.y}), slSpr.setOrigin({z.x * .5f, z.y * sYf}), slSpr.setPosition({sX, sY}); // scale sling, set fork origin fraction, place at anchor
    b2WorldDef wd = b2DefaultWorldDef(); // Box2D world configuration template
    wd.gravity = {0, 9.8f}; // downward gravity in m/s²
    wd.hitEventThreshold = .25f; // minimum approach speed to register hit events
    b2WorldId Wd = b2CreateWorld(&wd); // create simulation world instance
    stBox(Wd, {W * .5f, H - gH * .5f}, {W, gH}); // ground collider along bottom
    stBox(Wd, {40, H * .5f}, {80, H}); // left wall
    stBox(Wd, {W - 40, H * .5f}, {80, H}); // right wall
    stBox(Wd, {W * .5f, 0}, {W, 80}); // ceiling to keep bodies in frame
    const std::vector<Lvl> L = { // all level layouts (pig positions + blocks)
        {.pigs = {{980, 520}}, .blks = {{{900, 560}, {bW, bH}}, {{950, 560}, {bW, bH}}, {{1000, 560}, {bW, bH}}, {{900, 534}, {bW, bH}}, {{950, 534}, {bW, bH}}, {{1000, 534}, {bW, bH}}, {{900, 508}, {bW, bH}, Mat::Stone}, {{950, 508}, {bW, bH}, Mat::Stone}, {{1000, 508}, {bW, bH}, Mat::Stone}}}, // level 1
        {.pigs = {{1040, 480}}, .blks = {{{980, 570}, {bW, 120}, Mat::Stone}, {{1100, 570}, {bW, 120}, Mat::Stone}, {{1040, 520}, {180, bH}}, {{1040, 470}, {120, bH}}}}, // level 2
        {.pigs = {{1120, 560}}, .blks = {{{980, 580}, {260, bH}, Mat::Stone}, {{980, 550}, {260, bH}, Mat::Ice}, {{980, 520}, {260, bH}}, {{980, 490}, {260, bH}, Mat::Stone}}}, // level 3
        {.pigs = {{980, 560}, {1100, 560}}, .blks = {{{980, 600}, {220, bH}, Mat::Stone}, {{1100, 600}, {220, bH}, Mat::Stone}, {{1040, 540}, {320, bH}}, {{1040, 510}, {320, bH}, Mat::Ice}}}, // level 4 two pigs
        {.pigs = {{940, 560}, {1040, 560}, {1140, 560}}, .blks = {{{940, 600}, {120, bH}, Mat::Stone}, {{1040, 600}, {120, bH}, Mat::Stone}, {{1140, 600}, {120, bH}, Mat::Stone}, {{990, 540}, {220, bH}}, {{1090, 540}, {220, bH}}, {{1040, 510}, {380, bH}, Mat::Ice}}} // level 5 three pigs
    }; // end level table
    size_t li = 0; // current level index into L
    std::optional<Bird> B; // active bird (empty on fail until reload)
    std::vector<Pig> P; // all pigs this level
    std::vector<Blk> K; // all blocks this level
    std::vector<Tag> T; // parallel tags: index 0 bird, then pigs, then blocks (pointers stored in bodies)
    float dmgW = 0, idle = 0; // seconds left of post-spawn damage warmup; seconds bird has been nearly still
    const float gY = H - gH; // Y coordinate of ground top in pixels
    constexpr int maxB = 4; // birds (lives) per level
    int br = maxB, bu = 0, tot = 0, scg = 0; // birds remaining; birds used (launches); total score; last clear score delta
    bool vic = 0, fail = 0, pse = 0, vJ = 0, fJ = 0; // victory overlay; fail overlay; paused; victory jingle played; fail jingle played
    float vT = -1, fT = -1; // countdown timers for delayed end jingles (negative = inactive)
    int vst = 0; // stars earned on last clear (1..3)
    std::string vmsg; // rating message text for victory overlay
    sf::Font fU; // UI font for overlay and level HUD
    bool fOk = ldF(fU); // whether any font path succeeded
    auto stb = [&] { // pre-simulate level so tower settles, then sleep bodies
        constexpr float dt = 1 / 60.f; // fixed 60 Hz step for stabilization only
        int ss = 0; // consecutive steps where all bodies were slow enough
        for (int s = 0; s < 240; s++) { // up to 240 stabilization substeps
            b2World_Step(Wd, dt, 4); // advance physics with 4 velocity iterations
            bool sl = 1; // assume all slow until proven otherwise
            for (auto& p : P) { // check each pig’s speed
                if (!p.live || B2_IS_NULL(p.id)) continue; // skip dead or destroyed
                b2Vec2 v = b2Body_GetLinearVelocity(p.id); // linear velocity m/s
                if (v.x * v.x + v.y * v.y > st2) sl = 0; // above threshold → not settled
            } // end for pigs
            if (sl) for (auto& b : K) { // if pigs slow, also require blocks slow
                    if (B2_IS_NULL(b.id)) continue; // skip destroyed
                    b2Vec2 v = b2Body_GetLinearVelocity(b.id); // block velocity
                    if (v.x * v.x + v.y * v.y > st2) { sl = 0; break; } // any fast block breaks stability streak
                } // end for blocks
            ss = sl ? ss + 1 : 0; // increment streak or reset to zero
            if (ss >= 45) break; // ~0.75 s of stability → stop early
        } // end stabilization loop
        b2Vec2 z{}; // zero linear velocity vector
        for (auto& p : P) if (B2_IS_NON_NULL(p.id)) b2Body_SetLinearVelocity(p.id, z), b2Body_SetAngularVelocity(p.id, 0), b2Body_SetAwake(p.id, false); // freeze pigs
        for (auto& b : K) if (B2_IS_NON_NULL(b.id)) b2Body_SetLinearVelocity(b.id, z), b2Body_SetAngularVelocity(b.id, 0), b2Body_SetAwake(b.id, false); // freeze blocks
    }; // end stb lambda
    auto load = [&](size_t ix) { // destroy old bodies, build level ix, stabilize, reset UI timers
        vic = fail = vJ = fJ = 0, vst = 0, vmsg.clear(), scg = 0, bu = 0, vT = fT = -1, mEnd.stop(); // clear overlays, score line, usage, jingle state, stop music
        if (B) des(B->id); // remove old bird body if present
        for (auto& p : P) des(p.id); // destroy all pig bodies
        for (auto& b : K) des(b.id); // destroy all block bodies
        P.clear(), K.clear(), T.clear(), br = maxB, idle = 0; // empty vectors and reset lives and idle timer
        T.reserve(1 + L[ix].pigs.size() + L[ix].blks.size()); // one tag slot per bird + pig + block
        T.push_back({Kd::Bird, Mat::Stone, -1}); // tag index 0 reserved for bird (index field unused)
        B.emplace(mkBird(Wd, &T[0], tB)); // spawn new bird at slingshot
        for (size_t i = 0; i < L[ix].pigs.size(); i++) { // for each pig spawn in level data
            sf::Vector2f c = L[ix].pigs[i]; // desired pig center in pixels
            c.y = std::max(c.y, gY - pR); // clamp so pig sits on/above ground line
            T.push_back({Kd::Pig, Mat::Ice, (int)P.size()}); // new tag pointing at next pig index
            P.push_back(mkPig(Wd, c, &T.back(), b2_dynamicBody, tP)); // create pig body using pig texture
        } // end pig spawn loop
        for (auto& s : L[ix].blks) { // each block specification
            sf::Vector2f c = s.c; // copy center so we can clamp Y
            c.y = std::max(c.y, gY - s.s.y * .5f); // keep block bottom on or above ground
            T.push_back(Tag{Kd::Blok, s.m, -1}); // block tag carries material; index unused
            K.push_back(mkBlk(Wd, c, s.s, s.m, &T.back(), b2_dynamicBody)); // dynamic box with material color
        } // end block loop
        dmgW = wUp; // start post-load warmup so stabilization collisions don’t kill pigs
        stb(); // run settle pass
    }; // end load lambda
    load(li); // load first level
    if (sI) sI->play(); // play intro at startup if loaded
    sf::RectangleShape gr({W, gH}); // green ground strip drawable
    gr.setPosition({0, H - gH}); // align bottom of window
    gr.setFillColor({80, 140, 80}); // grass-like fill
    sf::VertexArray ln(sf::PrimitiveType::Lines, 2); // two vertices for slingshot aim line
    ln[0].color = ln[1].color = {40, 40, 40, 180}; // semi-transparent dark gray for aim line
    sf::Clock clk; // measures real time between frames for dt
    bool frz = 0; // true when paused or end overlay blocks gameplay/physics
    auto ttl = [&] { // refresh window title bar as lightweight HUD
        if (vic) return win.setTitle("Clear " + std::to_string(vst) + "* Score " + std::to_string(tot) + " [Space] next"), void(); // victory title + comma-void pattern
        if (fail) return win.setTitle("No birds 0* [Space] retry"), void(); // fail state title
        if (pse) return win.setTitle("Paused [Esc][R][Q]"), void(); // paused menu hint
        int ap = 0; // alive pig count
        for (auto& p : P) if (p.live) ap++; // tally living pigs
        win.setTitle("L" + std::to_string((int)li + 1) + " P" + std::to_string(ap) + " B" + std::to_string(br) + " S" + std::to_string(tot)); // compact in-game HUD
    }; // end ttl lambda
    ttl(); // set initial title
    while (win.isOpen()) { // main loop until window closed
        while (auto e = win.pollEvent()) { // drain all pending OS/window events this frame
            if (e->is<sf::Event::Closed>()) win.close(); // user clicked X → close window
            else if (auto k = e->getIf<sf::Event::KeyPressed>()) { // keyboard down event
                if (fail && (k->code == sf::Keyboard::Key::Space || k->code == sf::Keyboard::Key::Enter)) load(li), pse = 0, ttl(); // retry same level after fail
                else if (vic && (k->code == sf::Keyboard::Key::Space || k->code == sf::Keyboard::Key::Enter)) li = (li + 1) % L.size(), load(li), pse = 0, ttl(); // advance cyclic and load next
                else if (k->code == sf::Keyboard::Key::Escape && !vic && !fail) pse = !pse, ttl(); // toggle pause when not on end screens
                else if (pse && k->code == sf::Keyboard::Key::R) load(li), pse = 0, ttl(); // restart current level from pause
                else if (pse && k->code == sf::Keyboard::Key::Q) win.close(); // quit from pause menu
            } else if (pse || vic || fail) continue; // skip mouse gameplay when paused or on overlays
            else if (const auto* pb = e->getIf<sf::Event::MouseButtonPressed>()) { // mouse button went down
                if (B && !B->shot && pb->button == sf::Mouse::Button::Left) { // only if bird exists and not yet launched, left button
                    sf::Vector2f m((float)pb->position.x, (float)pb->position.y); // cursor in pixels
                    if (len(m - px(b2Body_GetPosition(B->id))) <= bR * 1.5f) B->drag = 1; // start drag if click near bird
                } // end if left on bird
            } else if (const auto* mm = e->getIf<sf::Event::MouseMoved>()) { // cursor moved
                if (B && B->drag) { // update slingshot pull while dragging
                    sf::Vector2f mp((float)mm->position.x, (float)mm->position.y); // new mouse position
                    B->dpx = clp({sX, sY}, mp); // clamp rubber band to max pull distance
                    b2Body_SetTransform(B->id, me(B->dpx), b2MakeRot(0)); // teleport physics body under cursor for aiming
                    b2Body_SetLinearVelocity(B->id, b2Vec2{0, 0}); // zero velocity while aiming
                    b2Body_SetAngularVelocity(B->id, 0); // no spin while aiming
                    b2Body_SetAwake(B->id, true); // keep body active for solver
                } // end if dragging
            } else if (const auto* pr = e->getIf<sf::Event::MouseButtonReleased>()) { // mouse button released
                if (B && B->drag && pr->button == sf::Mouse::Button::Left) { // release left ends drag → launch
                    B->drag = 0, B->shot = 1, bu++, br = std::max(0, br - 1), idle = 0, ttl(); // stop drag, mark launched, count shot and life, reset idle, HUD
                    b2Vec2 im = me(sf::Vector2f{sX, sY} - B->dpx); // pull vector in meters (anchor to bird)
                    im.x *= lStr, im.y *= lStr; // scale to impulse strength tuning constant lStr
                    b2Body_SetAwake(B->id, true); // ensure body participates in solver
                    b2Body_ApplyLinearImpulseToCenter(B->id, im, true); // apply launch impulse at center of mass
                    if (sL) sL->play(); // launch sound if buffer loaded
                } // end if release while dragging
            } // end mouse event chain
        } // end pollEvent loop
        float dt = clk.restart().asSeconds(); // frame delta time in seconds since last frame
        frz = pse || vic || fail; // freeze simulation during pause or end overlays
        if (vic && !vJ) { // victory screen active and stars jingle not yet fired
            vT -= dt; // count down delay before stars.mp3
            if (vT <= 0) ply("stars.mp3"), vJ = 1; // play streamed sting once delay elapsed
        } // end victory jingle
        if (fail && !fJ) { // fail overlay and fail jingle pending
            fT -= dt; // count down before wawawawa.mp3
            if (fT <= 0) ply("wawawawa.mp3"), fJ = 1; // play fail sting once
        } // end fail jingle
        if (!frz) { // normal gameplay update
            dmgW = std::max(0.f, dmgW - dt); // decay warmup timer toward zero
            b2World_Step(Wd, dt, 4); // integrate physics for this frame
            auto ev = b2World_GetContactEvents(Wd); // hit events produced by last step
            for (int i = 0; i < ev.hitCount; i++) { // iterate each hit
                auto& h = ev.hitEvents[i]; // reference to one hit record
                if (!b2Shape_IsValid(h.shapeIdA) || !b2Shape_IsValid(h.shapeIdB)) continue; // skip invalid pair
                b2BodyId a = b2Shape_GetBody(h.shapeIdA), b = b2Shape_GetBody(h.shapeIdB); // bodies touching this hit
                auto* ta = (Tag*)b2Body_GetUserData(a), *tb = (Tag*)b2Body_GetUserData(b); // user pointers cast to Tag
                if (!ta || !tb) continue; // skip if either body untagged
                bool pa = ta->k == Kd::Pig, pb = tb->k == Kd::Pig; // which side is pig
                if (!pa && !pb) continue; // ignore non-pig contacts for pig damage
                const Tag &pg = pa ? *ta : *tb, &ot = pa ? *tb : *ta; // pg always pig’s tag, ot the other body’s tag
                if (pg.ix < 0 || pg.ix >= (int)P.size()) continue; // bounds-check pig index
                Pig& pig = P[(size_t)pg.ix]; // reference hit pig in array
                if (!pig.live) continue; // skip dead pigs
                float dm = ot.k == Kd::Bird ? bDmg : (ot.k == Kd::Blok && dmgW <= 0 ? h.approachSpeed * 18.f * mdm(ot.m) : 0); // bird one-shots; blocks crush after warmup
                if (dm > 0 && (pig.hp -= dm) <= 0) pig.hp = 0, pig.live = 0, sP ? sP->play() : void(), ttl(); // apply damage; kill and SFX if depleted
            } // end hit loop
        } // end if !frz physics
        if (!frz && B && B->shot) for (auto& p : P) if (p.live && ov(B->id, bR, p.id, pR)) p.live = 0, p.hp = 0, sP ? sP->play() : void(), ttl(); // backup overlap kill bird vs pig
        if (!frz && !fail && !vic && !anyP(P)) { // all pigs dead while still in normal play
            Rate r = rate(bu); // compute stars/message from shots used
            vst = r.st, vmsg = r.msg, scg = vst * sStep, tot += scg, vic = 1, pse = 0, vJ = 0, vT = vsd, ttl(); // set victory state, score, delayed jingle timer, HUD
        } // end win detection
        if (!frz && B && B->shot && B2_IS_NON_NULL(B->id)) { // active launched bird with valid body
            sf::Vector2f bp = px(b2Body_GetPosition(B->id)); // bird center in pixels
            bool oob = bp.x < -200 || bp.x > W + 200 || bp.y < -200 || bp.y > H + 200; // generous off-screen margin
            b2Vec2 v = b2Body_GetLinearVelocity(B->id); // current velocity
            idle = (v.x * v.x + v.y * v.y < bWk2) ? idle + dt : 0; // accumulate time nearly stopped or reset if moving
            if (oob || idle > 1.25f) { // despawn bird when out of world or idle long enough
                des(B->id), B.reset(); // destroy physics body and clear optional bird
                if (br > 0) B.emplace(mkBird(Wd, &T[0], tB)), ttl(); // respawn next bird if lives remain
                else if (anyP(P)) fail = 1, pse = 0, fJ = 0, fT = fjd, ttl(); // no lives but pigs left → fail overlay + delayed fail sound
                else load(li), ttl(); // edge: no lives and no pigs (shouldn’t happen) → reload level
            } // end despawn branch
        } // end bird lifecycle
        if (B) { // only sync/draw if bird exists
            if (B->drag) B->sp.setPosition(B->dpx); // while dragging, sprite follows mouse not physics interpolation
            else synG(B->sp, B->id); // after launch, sprite follows body transform
        } // end if B
        for (auto& p : P) if (p.live) synG(p.sp, p.id); // sync living pig sprites to bodies
        for (auto& b : K) synS(b.sh, b.id); // sync every block rectangle to its body
        win.clear({150, 200, 255}); // clear color (visible if background fails to cover)
        win.draw(gSpr); // draw background image
        win.draw(gr); // draw ground strip on top of background
        win.draw(slSpr); // draw slingshot art at anchor
        if (B && B->drag) { // aim line + trajectory preview while dragging
            ln[0].position = {sX, sY}, ln[1].position = B->dpx, win.draw(ln); // rubber band line from anchor to bird
            b2Vec2 im = me(sf::Vector2f{sX, sY} - B->dpx), p0 = me(B->dpx); // impulse direction scaled later; start point in meters
            im.x *= lStr, im.y *= lStr; // same scaling as real launch for preview
            float mass = b2Body_GetMass(B->id); // bird mass from fixtures
            b2Vec2 v0 = mass > 1e-4f ? b2Vec2{im.x / mass, im.y / mass} : b2Vec2{}, g = b2World_GetGravity(Wd); // post-impulse v0 = impulse/mass; world gravity
            for (int i = 1; i <= 20; i++) { // draw several future samples
                float t = i * .08f; // time sample along parabola
                sf::CircleShape d(3); // small dot for trajectory
                d.setOrigin({3, 3}); // center dot on predicted point
                d.setPosition(px({p0.x + v0.x * t + .5f * g.x * t * t, p0.y + v0.y * t + .5f * g.y * t * t})); // ballistic position p0 + v0 t + ½ g t²
                d.setFillColor({40, 40, 40, 140}); // faint trajectory dots
                win.draw(d); // submit dot
            } // end trajectory dots
        } // end drag preview
        if (B) win.draw(B->sp); // draw bird sprite when present
        for (auto& p : P) if (p.live) win.draw(p.sp); // draw each living pig
        for (auto& b : K) win.draw(b.sh); // draw all blocks
        if (vic) drO(win, fOk ? &fU : nullptr, 1, vst, vmsg, scg, tot, ""); // victory overlay (failMsg unused)
        if (fail) drO(win, fOk ? &fU : nullptr, 0, 0, "", 0, 0, "7sl 5eir"); // zero-star fail overlay with fixed phrase
        if (fOk) { // HUD text needs font
            sf::Text lh(fU, "Level " + std::to_string((int)li + 1), 28u); // top-right level label
            lh.setFillColor(sf::Color::White); // bright text
            lh.setOutlineThickness(2); // stroke for readability on busy background
            lh.setOutlineColor({0, 0, 0, 200}); // dark semi-transparent outline
            auto lb = lh.getLocalBounds(); // measure text for right alignment
            lh.setOrigin({lb.position.x + lb.size.x, lb.position.y}); // origin at top-right of text box
            lh.setPosition({W - 18, 16}); // inset from top-right corner
            win.draw(lh); // draw level HUD
        } // end if fOk
        win.display(); // present back buffer to screen
    } // end while window open
    b2DestroyWorld(Wd); // free all Box2D memory tied to world
    return 0; // success exit code to OS
} // end main
