#include <SFML/Graphics.hpp>
#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr float kPixelsPerMeter = 30.0f;
constexpr float kWindowWidth = 1280.0f;
constexpr float kWindowHeight = 720.0f;
constexpr float kGroundHeight = 40.0f;
constexpr float kBirdRadiusPx = 18.0f;
constexpr float kPigRadiusPx = 20.0f;
constexpr float kBlockWidthPx = 48.0f;
constexpr float kBlockHeightPx = 24.0f;
constexpr float kSlingshotX = 220.0f;
constexpr float kSlingshotY = 520.0f;
constexpr float kMaxPullDistancePx = 140.0f;
constexpr float kLaunchStrength = 32.0f;
constexpr float kBirdOneShotDamage = 9999.0f;
constexpr float kPigMaxHealth = 200.0f;
constexpr float kLevelDamageWarmupSeconds = 0.75f;
constexpr float kBirdWakeThresholdSpeed2 = 0.04f;
constexpr float kStabilizeSpeedThreshold2 = 0.0025f; // (m/s)^2

sf::Vector2f ToPixels(const b2Vec2& meters)
{
    return { meters.x * kPixelsPerMeter, meters.y * kPixelsPerMeter };
}

b2Vec2 ToMeters(const sf::Vector2f& pixels)
{
    return b2Vec2{ pixels.x / kPixelsPerMeter, pixels.y / kPixelsPerMeter };
}

float Length(const sf::Vector2f& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

sf::Vector2f Normalize(const sf::Vector2f& v)
{
    const float len = Length(v);
    if (len <= 0.0001f)
    {
        return { 0.0f, 0.0f };
    }

    return { v.x / len, v.y / len };
}

sf::Vector2f ClampPull(const sf::Vector2f& origin, const sf::Vector2f& dragged)
{
    const sf::Vector2f delta = dragged - origin;
    const float distance = Length(delta);
    if (distance <= kMaxPullDistancePx)
    {
        return dragged;
    }

    return origin + Normalize(delta) * kMaxPullDistancePx;
}

struct Bird
{
    b2BodyId bodyId = b2_nullBodyId;
    sf::CircleShape shape;
    bool launched = false;
    bool dragging = false;
    sf::Vector2f dragPositionPx;
};

struct Block
{
    b2BodyId bodyId = b2_nullBodyId;
    sf::RectangleShape shape;
};

enum class Material : std::uint8_t
{
    Ice,
    Wood,
    Stone,
};

enum class Kind : std::uint8_t
{
    Unknown,
    Bird,
    Pig,
    Block,
};

struct BodyTag
{
    Kind kind = Kind::Unknown;
    Material material = Material::Ice;
    int index = -1; // pig index for Kind::Pig, unused otherwise
};

struct Pig
{
    b2BodyId bodyId = b2_nullBodyId;
    sf::CircleShape shape;
    bool alive = true;
    float health = 100.0f;
};

struct BlockSpec
{
    sf::Vector2f centerPx;
    sf::Vector2f sizePx;
    Material material = Material::Wood;
};

struct Level
{
    std::vector<sf::Vector2f> pigCentersPx;
    std::vector<BlockSpec> blocks;
};

sf::Color MaterialColor(Material material)
{
    switch (material)
    {
    case Material::Stone:
        return sf::Color(150, 150, 160);
    case Material::Wood:
        return sf::Color(160, 110, 60);
    case Material::Ice:
    default:
        return sf::Color(170, 220, 255);
    }
}

float MaterialDamageMultiplier(Material material)
{
    switch (material)
    {
    case Material::Stone:
        return 3.0f;
    case Material::Wood:
        return 2.0f;
    case Material::Ice:
    default:
        return 1.0f;
    }
}

b2BodyId CreateStaticBoxBody(b2WorldId worldId, const sf::Vector2f& centerPx, const sf::Vector2f& sizePx)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_staticBody;
    bodyDef.position = ToMeters(centerPx);
    bodyDef.rotation = b2MakeRot(0.0f);

    const b2BodyId bodyId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.8f;
    shapeDef.material.restitution = 0.1f;
    shapeDef.density = 0.0f;

    const float halfWidth = (sizePx.x * 0.5f) / kPixelsPerMeter;
    const float halfHeight = (sizePx.y * 0.5f) / kPixelsPerMeter;
    const b2Polygon box = b2MakeBox(halfWidth, halfHeight);
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    return bodyId;
}

Bird CreateBird(b2WorldId worldId, BodyTag* tag)
{
    Bird bird;

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = ToMeters(sf::Vector2f{ kSlingshotX, kSlingshotY });
    bodyDef.rotation = b2MakeRot(0.0f);
    bodyDef.linearDamping = 0.15f;
    bodyDef.angularDamping = 0.4f;
    bodyDef.userData = tag;

    bird.bodyId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 3.5f;
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.2f;
    shapeDef.enableHitEvents = true;

    b2Circle circle{};
    circle.center = b2Vec2{ 0.0f, 0.0f };
    circle.radius = kBirdRadiusPx / kPixelsPerMeter;
    b2CreateCircleShape(bird.bodyId, &shapeDef, &circle);

    bird.shape = sf::CircleShape(kBirdRadiusPx);
    bird.shape.setOrigin(sf::Vector2f{ kBirdRadiusPx, kBirdRadiusPx });
    bird.shape.setFillColor(sf::Color(220, 50, 50));

    // Placeholder texture code for a sprite-based version:
    // sf::Texture birdTexture;
    // birdTexture.loadFromFile("assets/bird.png");
    // sf::Sprite birdSprite;
    // birdSprite.setTexture(birdTexture);

    bird.dragPositionPx = { kSlingshotX, kSlingshotY };
    return bird;
}

Block CreateBlock(b2WorldId worldId, const sf::Vector2f& centerPx, const sf::Vector2f& sizePx, Material material, BodyTag* tag, b2BodyType bodyType)
{
    Block block;

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = bodyType;
    bodyDef.position = ToMeters(centerPx);
    bodyDef.rotation = b2MakeRot(0.0f);
    bodyDef.linearDamping = 0.05f;
    bodyDef.angularDamping = 0.2f;
    bodyDef.userData = tag;

    block.bodyId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 0.8f;
    shapeDef.material.friction = 0.7f;
    shapeDef.material.restitution = 0.05f;
    shapeDef.enableHitEvents = true;

    const float halfWidth = (sizePx.x * 0.5f) / kPixelsPerMeter;
    const float halfHeight = (sizePx.y * 0.5f) / kPixelsPerMeter;
    const b2Polygon box = b2MakeBox(halfWidth, halfHeight);
    b2CreatePolygonShape(block.bodyId, &shapeDef, &box);

    block.shape = sf::RectangleShape(sizePx);
    block.shape.setOrigin(sf::Vector2f{ sizePx.x * 0.5f, sizePx.y * 0.5f });
    block.shape.setFillColor(MaterialColor(material));

    // Placeholder texture code for a sprite-based version:
    // sf::Texture woodTexture;
    // woodTexture.loadFromFile("assets/wood.png");
    // sf::Sprite woodSprite;
    // woodSprite.setTexture(woodTexture);

    return block;
}

Pig CreatePig(b2WorldId worldId, const sf::Vector2f& centerPx, BodyTag* tag, b2BodyType bodyType)
{
    Pig pig;

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = bodyType;
    bodyDef.position = ToMeters(centerPx);
    bodyDef.rotation = b2MakeRot(0.0f);
    bodyDef.linearDamping = 0.25f;
    bodyDef.angularDamping = 0.35f;
    bodyDef.userData = tag;

    pig.bodyId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.2f;
    shapeDef.material.friction = 0.6f;
    shapeDef.material.restitution = 0.15f;
    shapeDef.enableHitEvents = true;

    b2Circle circle{};
    circle.center = b2Vec2{ 0.0f, 0.0f };
    circle.radius = kPigRadiusPx / kPixelsPerMeter;
    b2CreateCircleShape(pig.bodyId, &shapeDef, &circle);

    pig.shape = sf::CircleShape(kPigRadiusPx);
    pig.shape.setOrigin(sf::Vector2f{ kPigRadiusPx, kPigRadiusPx });
    pig.shape.setFillColor(sf::Color(70, 200, 70));
    pig.alive = true;
    pig.health = kPigMaxHealth;

    return pig;
}

void SyncShapeWithBody(sf::Shape& shape, b2BodyId bodyId)
{
    const b2Vec2 position = b2Body_GetPosition(bodyId);
    shape.setPosition(ToPixels(position));

    const b2Rot rot = b2Body_GetRotation(bodyId);
    const float angleRadians = std::atan2(rot.s, rot.c);
    shape.setRotation(sf::radians(angleRadians));
}

bool AreBodiesOverlappingAsCircles(b2BodyId a, float aRadiusPx, b2BodyId b, float bRadiusPx)
{
    const sf::Vector2f aPx = ToPixels(b2Body_GetPosition(a));
    const sf::Vector2f bPx = ToPixels(b2Body_GetPosition(b));
    const float r = aRadiusPx + bRadiusPx;
    const sf::Vector2f d = aPx - bPx;
    return (d.x * d.x + d.y * d.y) <= (r * r);
}

void DestroyIfValid(b2BodyId& bodyId)
{
    if (B2_IS_NON_NULL(bodyId))
    {
        b2DestroyBody(bodyId);
        bodyId = b2_nullBodyId;
    }
}
} // namespace

int main()
{
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(static_cast<unsigned int>(kWindowWidth), static_cast<unsigned int>(kWindowHeight))),
        "Angry Birds Prototype"
    );
    window.setFramerateLimit(60);

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = b2Vec2{ 0.0f, 9.8f };
    worldDef.hitEventThreshold = 0.25f;
    const b2WorldId worldId = b2CreateWorld(&worldDef);

    // Ground and level geometry.
    CreateStaticBoxBody(worldId, { kWindowWidth * 0.5f, kWindowHeight - (kGroundHeight * 0.5f) }, { kWindowWidth, kGroundHeight });
    CreateStaticBoxBody(worldId, { 40.0f, kWindowHeight * 0.5f }, { 80.0f, kWindowHeight });
    CreateStaticBoxBody(worldId, { kWindowWidth - 40.0f, kWindowHeight * 0.5f }, { 80.0f, kWindowHeight });
    // Ceiling (prevents launching into infinity).
    CreateStaticBoxBody(worldId, { kWindowWidth * 0.5f, 0.0f }, { kWindowWidth, 80.0f });

    const std::vector<Level> levels = {
        Level{
            .pigCentersPx = { sf::Vector2f{ 980.0f, 520.0f } },
            .blocks = {
                { { 900.0f, 560.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 950.0f, 560.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 1000.0f, 560.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 900.0f, 534.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 950.0f, 534.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 1000.0f, 534.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Wood },
                { { 900.0f, 508.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Stone },
                { { 950.0f, 508.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Stone },
                { { 1000.0f, 508.0f }, { kBlockWidthPx, kBlockHeightPx }, Material::Stone },
            },
        },
        Level{
            .pigCentersPx = { sf::Vector2f{ 1040.0f, 480.0f } },
            .blocks = {
                { { 980.0f, 570.0f }, { kBlockWidthPx, 120.0f }, Material::Stone },
                { { 1100.0f, 570.0f }, { kBlockWidthPx, 120.0f }, Material::Stone },
                { { 1040.0f, 520.0f }, { 180.0f, kBlockHeightPx }, Material::Wood },
                { { 1040.0f, 470.0f }, { 120.0f, kBlockHeightPx }, Material::Wood },
            },
        },
        Level{
            .pigCentersPx = { sf::Vector2f{ 1120.0f, 560.0f } },
            .blocks = {
                { { 980.0f, 580.0f }, { 260.0f, kBlockHeightPx }, Material::Stone },
                { { 980.0f, 550.0f }, { 260.0f, kBlockHeightPx }, Material::Ice },
                { { 980.0f, 520.0f }, { 260.0f, kBlockHeightPx }, Material::Wood },
                { { 980.0f, 490.0f }, { 260.0f, kBlockHeightPx }, Material::Stone },
            },
        },
        Level{
            .pigCentersPx = { sf::Vector2f{ 980.0f, 560.0f }, sf::Vector2f{ 1100.0f, 560.0f } },
            .blocks = {
                { { 980.0f, 600.0f }, { 220.0f, kBlockHeightPx }, Material::Stone },
                { { 1100.0f, 600.0f }, { 220.0f, kBlockHeightPx }, Material::Stone },
                { { 1040.0f, 540.0f }, { 320.0f, kBlockHeightPx }, Material::Wood },
                { { 1040.0f, 510.0f }, { 320.0f, kBlockHeightPx }, Material::Ice },
            },
        },
        Level{
            .pigCentersPx = { sf::Vector2f{ 940.0f, 560.0f }, sf::Vector2f{ 1040.0f, 560.0f }, sf::Vector2f{ 1140.0f, 560.0f } },
            .blocks = {
                { { 940.0f, 600.0f }, { 120.0f, kBlockHeightPx }, Material::Stone },
                { { 1040.0f, 600.0f }, { 120.0f, kBlockHeightPx }, Material::Stone },
                { { 1140.0f, 600.0f }, { 120.0f, kBlockHeightPx }, Material::Stone },
                { { 990.0f, 540.0f }, { 220.0f, kBlockHeightPx }, Material::Wood },
                { { 1090.0f, 540.0f }, { 220.0f, kBlockHeightPx }, Material::Wood },
                { { 1040.0f, 510.0f }, { 380.0f, kBlockHeightPx }, Material::Ice },
            },
        },
    };

    std::size_t currentLevelIndex = 0;

    Bird bird;
    std::vector<Pig> pigs;
    std::vector<Block> blocks;
    std::vector<BodyTag> tags;
    float damageWarmupRemaining = 0.0f;

    const float groundTopY = kWindowHeight - kGroundHeight;
    constexpr int kMaxBirdsPerLevel = 4;
    int birdsRemaining = kMaxBirdsPerLevel;
    float birdIdleSeconds = 0.0f;
    auto stabilizeLevel = [&]()
    {
        // Pre-simulate until the structure naturally settles, then start the level asleep and stable.
        constexpr float kFixedDt = 1.0f / 60.0f;
        constexpr int kMaxSteps = 240;           // up to 4 seconds
        constexpr int kRequiredStableSteps = 45; // ~0.75 seconds stable

        int stableSteps = 0;
        for (int step = 0; step < kMaxSteps; ++step)
        {
            b2World_Step(worldId, kFixedDt, 4);

            bool allSlow = true;
            for (const Pig& p : pigs)
            {
                if (!p.alive || B2_IS_NULL(p.bodyId))
                    continue;

                const b2Vec2 v = b2Body_GetLinearVelocity(p.bodyId);
                const float s2 = v.x * v.x + v.y * v.y;
                if (s2 > kStabilizeSpeedThreshold2)
                {
                    allSlow = false;
                    break;
                }
            }

            if (allSlow)
            {
                for (const Block& b : blocks)
                {
                    if (B2_IS_NULL(b.bodyId))
                        continue;

                    const b2Vec2 v = b2Body_GetLinearVelocity(b.bodyId);
                    const float s2 = v.x * v.x + v.y * v.y;
                    if (s2 > kStabilizeSpeedThreshold2)
                    {
                        allSlow = false;
                        break;
                    }
                }
            }

            stableSteps = allSlow ? (stableSteps + 1) : 0;
            if (stableSteps >= kRequiredStableSteps)
            {
                break;
            }
        }

        // Forcefully remove any remaining jitter and put bodies to sleep.
        for (Pig& p : pigs)
        {
            if (B2_IS_NON_NULL(p.bodyId))
            {
                b2Body_SetLinearVelocity(p.bodyId, b2Vec2{ 0.0f, 0.0f });
                b2Body_SetAngularVelocity(p.bodyId, 0.0f);
                b2Body_SetAwake(p.bodyId, false);
            }
        }
        for (Block& b : blocks)
        {
            if (B2_IS_NON_NULL(b.bodyId))
            {
                b2Body_SetLinearVelocity(b.bodyId, b2Vec2{ 0.0f, 0.0f });
                b2Body_SetAngularVelocity(b.bodyId, 0.0f);
                b2Body_SetAwake(b.bodyId, false);
            }
        }
    };

    auto loadLevel = [&](std::size_t levelIndex)
    {
        DestroyIfValid(bird.bodyId);
        for (Pig& p : pigs)
        {
            DestroyIfValid(p.bodyId);
        }
        for (Block& b : blocks)
        {
            DestroyIfValid(b.bodyId);
        }
        pigs.clear();
        blocks.clear();
        tags.clear();

        birdsRemaining = kMaxBirdsPerLevel;
        birdIdleSeconds = 0.0f;

        tags.reserve(1 + static_cast<std::size_t>(levels[levelIndex].pigCentersPx.size()) + levels[levelIndex].blocks.size());
        tags.push_back(BodyTag{ .kind = Kind::Bird, .material = Material::Stone, .index = -1 });
        bird = CreateBird(worldId, &tags[0]);

        // Pigs (up to 4).
        pigs.reserve(levels[levelIndex].pigCentersPx.size());
        for (std::size_t i = 0; i < levels[levelIndex].pigCentersPx.size(); ++i)
        {
            sf::Vector2f pigPos = levels[levelIndex].pigCentersPx[i];
            const float pigMinY = groundTopY - kPigRadiusPx;
            pigPos.y = std::max(pigPos.y, pigMinY);

            tags.push_back(BodyTag{ .kind = Kind::Pig, .material = Material::Ice, .index = static_cast<int>(pigs.size()) });
            pigs.push_back(CreatePig(worldId, pigPos, &tags.back(), b2_dynamicBody));
        }

        blocks.reserve(levels[levelIndex].blocks.size());
        for (const BlockSpec& spec : levels[levelIndex].blocks)
        {
            sf::Vector2f centerPx = spec.centerPx;
            const float halfH = spec.sizePx.y * 0.5f;
            const float minCenterY = groundTopY - halfH;
            // Force blocks down onto the ground (never floating).
            centerPx.y = std::max(centerPx.y, minCenterY);

            tags.push_back(BodyTag{ .kind = Kind::Block, .material = spec.material, .index = -1 });
            blocks.push_back(CreateBlock(worldId, centerPx, spec.sizePx, spec.material, &tags.back(), b2_dynamicBody));
        }

        // Settle the structure into its final stable resting state before the player starts.
        damageWarmupRemaining = kLevelDamageWarmupSeconds;
        stabilizeLevel();
    };

    loadLevel(currentLevelIndex);

    sf::RectangleShape groundShape({ kWindowWidth, kGroundHeight });
    groundShape.setPosition(sf::Vector2f{ 0.0f, kWindowHeight - kGroundHeight });
    groundShape.setFillColor(sf::Color(80, 140, 80));

    sf::RectangleShape slingBase(sf::Vector2f(10.0f, 90.0f));
    slingBase.setOrigin(sf::Vector2f{ 5.0f, 90.0f });
    slingBase.setPosition(sf::Vector2f{ kSlingshotX - 20.0f, kSlingshotY + 10.0f });
    slingBase.setFillColor(sf::Color(120, 80, 40));

    sf::VertexArray aimLine(sf::PrimitiveType::Lines, 2);
    aimLine[0].color = sf::Color(40, 40, 40, 180);
    aimLine[1].color = sf::Color(40, 40, 40, 180);

    sf::Clock clock;

    bool paused = false;
    auto updateTitle = [&]()
    {
        if (paused)
        {
            window.setTitle("Paused - [Esc] Resume  [R] Restart  [Q] Quit");
            return;
        }

        int alivePigs = 0;
        for (const Pig& p : pigs)
        {
            if (p.alive)
            {
                ++alivePigs;
            }
        }

        window.setTitle(
            "Angry Birds Prototype - Level " + std::to_string(static_cast<int>(currentLevelIndex + 1)) +
            " - Pigs: " + std::to_string(alivePigs) +
            " - Birds: " + std::to_string(birdsRemaining)
        );
    };

    updateTitle();

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    paused = !paused;
                    updateTitle();
                }
                else if (paused && (key->code == sf::Keyboard::Key::R))
                {
                    loadLevel(currentLevelIndex);
                    paused = false;
                    updateTitle();
                }
                else if (paused && (key->code == sf::Keyboard::Key::Q))
                {
                    window.close();
                }
            }
            else if (paused)
            {
                // Ignore gameplay input while paused.
                continue;
            }
            else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (!bird.launched && pressed->button == sf::Mouse::Button::Left)
                {
                    const sf::Vector2f mousePx(static_cast<float>(pressed->position.x), static_cast<float>(pressed->position.y));
                    const sf::Vector2f birdCenterPx = ToPixels(b2Body_GetPosition(bird.bodyId));
                    const float distance = Length(mousePx - birdCenterPx);
                    if (distance <= kBirdRadiusPx * 1.5f)
                    {
                        bird.dragging = true;
                    }
                }
            }
            else if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
            {
                if (bird.dragging)
                {
                    const sf::Vector2f mousePx(static_cast<float>(moved->position.x), static_cast<float>(moved->position.y));
                    bird.dragPositionPx = ClampPull({ kSlingshotX, kSlingshotY }, mousePx);
                    b2Body_SetTransform(bird.bodyId, ToMeters(bird.dragPositionPx), b2MakeRot(0.0f));
                    b2Body_SetLinearVelocity(bird.bodyId, b2Vec2{ 0.0f, 0.0f });
                    b2Body_SetAngularVelocity(bird.bodyId, 0.0f);
                    b2Body_SetAwake(bird.bodyId, true);
                }
            }
            else if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (bird.dragging && released->button == sf::Mouse::Button::Left)
                {
                    bird.dragging = false;
                    bird.launched = true;
                    birdsRemaining = std::max(0, birdsRemaining - 1);
                    birdIdleSeconds = 0.0f;
                    updateTitle();

                    const sf::Vector2f slingshotOriginPx(kSlingshotX, kSlingshotY);
                    const sf::Vector2f pullPx = slingshotOriginPx - bird.dragPositionPx;
                    const b2Vec2 pullMeters = ToMeters(pullPx);
                    const b2Vec2 impulse = b2Vec2{ pullMeters.x * kLaunchStrength, pullMeters.y * kLaunchStrength };
                    b2Body_SetAwake(bird.bodyId, true);
                    b2Body_ApplyLinearImpulseToCenter(bird.bodyId, impulse, true);
                }
            }
        }

        const float dt = clock.restart().asSeconds();
        if (!paused)
        {
            damageWarmupRemaining = std::max(0.0f, damageWarmupRemaining - dt);
            b2World_Step(worldId, dt, 4);

            // Damage system using Box2D hit events.
            const b2ContactEvents contactEvents = b2World_GetContactEvents(worldId);
            for (int i = 0; i < contactEvents.hitCount; ++i)
            {
                const b2ContactHitEvent& hit = contactEvents.hitEvents[i];

                if (!b2Shape_IsValid(hit.shapeIdA) || !b2Shape_IsValid(hit.shapeIdB))
                {
                    continue;
                }

                const b2BodyId bodyA = b2Shape_GetBody(hit.shapeIdA);
                const b2BodyId bodyB = b2Shape_GetBody(hit.shapeIdB);

                const auto* tagA = static_cast<const BodyTag*>(b2Body_GetUserData(bodyA));
                const auto* tagB = static_cast<const BodyTag*>(b2Body_GetUserData(bodyB));

                if (!tagA || !tagB)
                {
                    continue;
                }

                const bool aIsPig = (tagA->kind == Kind::Pig);
                const bool bIsPig = (tagB->kind == Kind::Pig);
                if ((!aIsPig && !bIsPig))
                {
                    continue;
                }

                const BodyTag& pigTag = aIsPig ? *tagA : *tagB;
                const BodyTag& otherTag = aIsPig ? *tagB : *tagA;

                if (pigTag.index < 0 || pigTag.index >= static_cast<int>(pigs.size()))
                {
                    continue;
                }
                Pig& hitPig = pigs[static_cast<std::size_t>(pigTag.index)];
                if (!hitPig.alive)
                {
                    continue;
                }

                float damage = 0.0f;
                if (otherTag.kind == Kind::Bird)
                {
                    damage = kBirdOneShotDamage;
                }
                else if (otherTag.kind == Kind::Block)
                {
                    if (damageWarmupRemaining > 0.0f)
                    {
                        continue;
                    }
                    // Convert approach speed into damage with a material multiplier.
                    // Tuned for "game feel", not real physics.
                    damage = hit.approachSpeed * 18.0f * MaterialDamageMultiplier(otherTag.material);
                }

                if (damage > 0.0f)
                {
                    hitPig.health -= damage;
                    if (hitPig.health <= 0.0f)
                    {
                        hitPig.health = 0.0f;
                        hitPig.alive = false;
                        updateTitle();
                    }
                }
            }
        }

        // Bird one-shots pigs on contact.
        if (!paused && bird.launched)
        {
            for (Pig& p : pigs)
            {
                if (p.alive && AreBodiesOverlappingAsCircles(bird.bodyId, kBirdRadiusPx, p.bodyId, kPigRadiusPx))
                {
                    p.alive = false;
                    p.health = 0.0f;
                    updateTitle();
                }
            }
        }

        // Win: all pigs eliminated -> next level.
        if (!paused)
        {
            bool anyAlive = false;
            for (const Pig& p : pigs)
            {
                if (p.alive)
                {
                    anyAlive = true;
                    break;
                }
            }

            if (!anyAlive)
            {
                currentLevelIndex = (currentLevelIndex + 1) % levels.size();
                loadLevel(currentLevelIndex);
                updateTitle();
            }
        }

        // Lose/next bird: if bird is out of bounds or comes to rest.
        if (!paused && bird.launched && B2_IS_NON_NULL(bird.bodyId))
        {
            const sf::Vector2f birdPx = ToPixels(b2Body_GetPosition(bird.bodyId));
            const float margin = 200.0f;
            const bool outOfBounds =
                birdPx.x < -margin || birdPx.x > (kWindowWidth + margin) ||
                birdPx.y < -margin || birdPx.y > (kWindowHeight + margin);

            const b2Vec2 v = b2Body_GetLinearVelocity(bird.bodyId);
            const float speed2 = v.x * v.x + v.y * v.y;
            if (speed2 < kBirdWakeThresholdSpeed2)
            {
                birdIdleSeconds += dt;
            }
            else
            {
                birdIdleSeconds = 0.0f;
            }

            if (outOfBounds || birdIdleSeconds > 1.25f)
            {
                // Remove used bird and spawn a new one if available; otherwise restart level.
                DestroyIfValid(bird.bodyId);
                bird = Bird();

                if (birdsRemaining > 0)
                {
                    // Recreate bird body using existing bird tag (tags[0]).
                    bird = CreateBird(worldId, &tags[0]);
                    updateTitle();
                }
                else
                {
                    // Out of lives: restart this level.
                    loadLevel(currentLevelIndex);
                    updateTitle();
                }
            }
        }

        if (bird.dragging)
        {
            bird.shape.setPosition(bird.dragPositionPx);
        }
        else
        {
            SyncShapeWithBody(bird.shape, bird.bodyId);
        }

        for (Pig& p : pigs)
        {
            if (p.alive)
            {
                SyncShapeWithBody(p.shape, p.bodyId);
            }
        }

        for (Block& block : blocks)
        {
            SyncShapeWithBody(block.shape, block.bodyId);
        }

        window.clear(sf::Color(150, 200, 255));
        window.draw(groundShape);
        window.draw(slingBase);

        // Aiming helpers (line + trajectory preview).
        if (bird.dragging)
        {
            const sf::Vector2f slingshotOriginPx(kSlingshotX, kSlingshotY);
            aimLine[0].position = slingshotOriginPx;
            aimLine[1].position = bird.dragPositionPx;
            window.draw(aimLine);

            // Trajectory prediction (simple ballistic approximation).
            const sf::Vector2f pullPx = slingshotOriginPx - bird.dragPositionPx;
            const b2Vec2 pullMeters = ToMeters(pullPx);
            const b2Vec2 impulse = b2Vec2{ pullMeters.x * kLaunchStrength, pullMeters.y * kLaunchStrength };
            const float mass = b2Body_GetMass(bird.bodyId);
            const b2Vec2 v0 = (mass > 0.0001f) ? b2Vec2{ impulse.x / mass, impulse.y / mass } : b2Vec2{ 0.0f, 0.0f };
            const b2Vec2 g = b2World_GetGravity(worldId);
            const b2Vec2 p0 = ToMeters(bird.dragPositionPx);

            constexpr int kDots = 20;
            constexpr float kStepSeconds = 0.08f;
            for (int i = 1; i <= kDots; ++i)
            {
                const float t = i * kStepSeconds;
                const b2Vec2 pt = b2Vec2{
                    p0.x + v0.x * t + 0.5f * g.x * t * t,
                    p0.y + v0.y * t + 0.5f * g.y * t * t,
                };
                const sf::Vector2f ptPx = ToPixels(pt);

                sf::CircleShape dot(3.0f);
                dot.setOrigin(sf::Vector2f{ 3.0f, 3.0f });
                dot.setPosition(ptPx);
                dot.setFillColor(sf::Color(40, 40, 40, 140));
                window.draw(dot);
            }
        }

        window.draw(bird.shape);
        for (const Pig& p : pigs)
        {
            if (p.alive)
            {
                window.draw(p.shape);
            }
        }
        for (const Block& block : blocks)
        {
            window.draw(block.shape);
        }
        window.display();
    }

    b2DestroyWorld(worldId);
    return 0;
}
