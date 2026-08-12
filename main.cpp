#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Vector2f previousPosition;
};

constexpr unsigned int WIDTH = 1280;
constexpr unsigned int HEIGHT = 720;

constexpr float G = 5000.0f;
constexpr float BLACK_HOLE_MASS = 1000.0f;
constexpr float EVENT_HORIZON = 18.0f;

constexpr int PARTICLES_PER_STEP = 1000;

float randomFloat(float min, float max)
{
    return min +
        static_cast<float>(std::rand()) / RAND_MAX *
        (max - min);
}

float length(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Particle createParticle(sf::Vector2f blackHole)
{
    float angle = randomFloat(0.0f, 6.2831853f);
    float radius = randomFloat(100.0f, 350.0f);

    Particle p;

    p.position =
    {
        blackHole.x + std::cos(angle) * radius,
        blackHole.y + std::sin(angle) * radius
    };

    p.previousPosition = p.position;

    float orbitalSpeed =
        std::sqrt(G * BLACK_HOLE_MASS / radius);

    p.velocity =
    {
        -std::sin(angle) * orbitalSpeed,
        std::cos(angle) * orbitalSpeed
    };

    float randomness = randomFloat(0.85f, 1.15f);

    p.velocity.x *= randomness;
    p.velocity.y *= randomness;

    return p;
}

void addParticles(
    std::vector<Particle>& particles,
    int amount,
    sf::Vector2f blackHole
)
{
    particles.reserve(particles.size() + amount);

    for (int i = 0; i < amount; ++i)
        particles.push_back(createParticle(blackHole));
}

sf::Color velocityColor(float speed)
{
    float t =
        std::clamp(speed / 250.0f, 0.0f, 1.0f);

    // Blue → purple → red → orange
    if (t < 0.33f)
    {
        float x = t / 0.33f;

        return sf::Color(
            static_cast<std::uint8_t>(50 + 100 * x),
            static_cast<std::uint8_t>(80 * x),
            255
        );
    }

    if (t < 0.66f)
    {
        float x = (t - 0.33f) / 0.33f;

        return sf::Color(
            static_cast<std::uint8_t>(150 + 105 * x),
            0,
            static_cast<std::uint8_t>(255 - 100 * x)
        );
    }

    float x = (t - 0.66f) / 0.34f;

    return sf::Color(
        255,
        static_cast<std::uint8_t>(80 + 175 * x),
        static_cast<std::uint8_t>(20 * (1.0f - x))
    );
}

struct HudMetrics
{
    std::size_t particleCount;
    float fps;
    float physicsMs;
    float renderMs;
    float zoom;
    bool paused;
    bool brightnessEnabled;
    bool diskEnabled;
    bool trailsEnabled;
    bool velocityColorsEnabled;
    bool glowEnabled;
};

std::string hudToggle(bool enabled)
{
    return enabled ? "on" : "off";
}

void drawHud(
    sf::RenderWindow& window,
    const sf::Font& font,
    const HudMetrics& m
)
{
    const float panelMargin = 16.0f;
    const float contentMargin = 16.0f;

    std::ostringstream stats;
    stats
        << "particles  " << m.particleCount << "\n"
        << "fps        "
        << std::fixed
        << std::setprecision(1)
        << m.fps << "\n"
        << "physics    "
        << std::fixed
        << std::setprecision(2)
        << m.physicsMs
        << " ms\n"
        << "render     "
        << std::fixed
        << std::setprecision(2)
        << m.renderMs
        << " ms\n"
        << "zoom       "
        << std::fixed
        << std::setprecision(1)
        << m.zoom
        << "x\n"
        << "state      "
        << (m.paused ? "paused" : "running");

    std::ostringstream toggles;
    toggles
        << "brightness  "
        << hudToggle(m.brightnessEnabled) << "\n"
        << "disk        "
        << hudToggle(m.diskEnabled) << "\n"
        << "trails      "
        << hudToggle(m.trailsEnabled) << "\n"
        << "velocity    "
        << hudToggle(m.velocityColorsEnabled) << "\n"
        << "glow        "
        << hudToggle(m.glowEnabled);

    const char* controls =
        "space  pause\n"
        "1-5    render toggles\n"
        "=/-    add/remove particles\n"
        "r      reset particles\n"
        "z/x    zoom in/out\n"
        "tab    show/hide hud\n"
        "esc    quit";

    sf::Text titleText(
        font,
        "BLACK HOLE LAB",
        22
    );

    sf::Text statsText(
        font,
        stats.str(),
        15
    );

    sf::Text togglesText(
        font,
        toggles.str(),
        15
    );

    sf::Text controlsText(
        font,
        controls,
        13
    );

    titleText.setFillColor(
        sf::Color(255, 166, 64)
    );

    statsText.setFillColor(
        sf::Color(225, 225, 230)
    );

    togglesText.setFillColor(
        sf::Color(225, 225, 230)
    );

    controlsText.setFillColor(
        sf::Color(155, 158, 168)
    );

    const float contentX = contentMargin;
    float y = contentMargin;

    titleText.setPosition(
        {contentX, y}
    );

    y += 34.0f;

    statsText.setPosition(
        {contentX, y}
    );

    const float togglesX =
        contentX +
        statsText.getLocalBounds().size.x +
        28.0f;

    togglesText.setPosition(
        {togglesX, y}
    );

    y +=
        statsText.getLocalBounds().size.y +
        18.0f;

    controlsText.setPosition(
        {contentX, y}
    );

    float width =
        std::max(
            togglesX +
                togglesText.getLocalBounds().size.x,
            contentX +
                controlsText.getLocalBounds().size.x
        ) +
        contentMargin;

    float height =
        y +
        controlsText.getLocalBounds().size.y +
        contentMargin;

    sf::RectangleShape panel(
        {width, height}
    );

    panel.setPosition(
        {panelMargin, panelMargin}
    );

    panel.setFillColor(
        sf::Color(8, 8, 16, 185)
    );

    panel.setOutlineThickness(
        1.0f
    );

    panel.setOutlineColor(
        sf::Color(255, 166, 64, 80)
    );

    window.draw(panel);
    window.draw(titleText);
    window.draw(statsText);
    window.draw(togglesText);
    window.draw(controlsText);
}

int main()
{
    std::srand(
        static_cast<unsigned>(std::time(nullptr))
    );

    sf::RenderWindow window(
        sf::VideoMode({WIDTH, HEIGHT}),
        "BLACK HOLE LAB"
    );

    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(0);

    sf::Font hudFont;

    if (!hudFont.openFromFile(
            "assets/liberation-mono.ttf"
        ))
    {
        std::fprintf(
            stderr,
            "warning: could not load hud font\n"
        );
    }

    sf::Vector2f blackHole(
        WIDTH / 2.0f,
        HEIGHT / 2.0f
    );

    std::vector<Particle> particles;

    addParticles(
        particles,
        10000,
        blackHole
    );

    // =========================================================
    // TOGGLES
    // =========================================================

    bool brightnessEnabled = true;
    bool diskEnabled = true;
    bool trailsEnabled = false;
    bool velocityColorsEnabled = false;
    bool glowEnabled = true;
    bool hudEnabled = true;

    bool paused = false;

    float zoom = 1.0f;

    // =========================================================
    // BLACK HOLE
    // =========================================================

    sf::CircleShape blackHoleShape(
        EVENT_HORIZON
    );

    blackHoleShape.setOrigin(
        {EVENT_HORIZON, EVENT_HORIZON}
    );

    blackHoleShape.setPosition(
        blackHole
    );

    blackHoleShape.setFillColor(
        sf::Color::Black
    );

    // =========================================================
    // PARTICLE RENDERING
    // =========================================================

    sf::VertexArray particleVertices(
        sf::PrimitiveType::Points
    );

    // =========================================================
    // TRAILS
    // =========================================================

    sf::VertexArray trailVertices(
        sf::PrimitiveType::Lines
    );

    // =========================================================
    // TIMING
    // =========================================================

    sf::Clock frameClock;
    sf::Clock fpsClock;

    float fps = 0.0f;
    float physicsMs = 0.0f;
    float renderMs = 0.0f;

    int frames = 0;

    while (window.isOpen())
    {
        // =====================================================
        // EVENTS
        // =====================================================

        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code ==
                    sf::Keyboard::Key::Escape)
                {
                    window.close();
                }

                if (key->code ==
                    sf::Keyboard::Key::Space)
                {
                    paused = !paused;
                }

                if (key->code ==
                    sf::Keyboard::Key::Num1)
                {
                    brightnessEnabled =
                        !brightnessEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Num2)
                {
                    diskEnabled =
                        !diskEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Num3)
                {
                    trailsEnabled =
                        !trailsEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Num4)
                {
                    velocityColorsEnabled =
                        !velocityColorsEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Num5)
                {
                    glowEnabled =
                        !glowEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Tab)
                {
                    hudEnabled =
                        !hudEnabled;
                }

                if (key->code ==
                    sf::Keyboard::Key::Equal)
                {
                    addParticles(
                        particles,
                        PARTICLES_PER_STEP,
                        blackHole
                    );
                }

                if (key->code ==
                    sf::Keyboard::Key::Hyphen)
                {
                    int remove =
                        std::min(
                            PARTICLES_PER_STEP,
                            static_cast<int>(
                                particles.size()
                            )
                        );

                    particles.resize(
                        particles.size() - remove
                    );
                }

                if (key->code ==
                    sf::Keyboard::Key::R)
                {
                    particles.clear();

                    addParticles(
                        particles,
                        10000,
                        blackHole
                    );
                }

                if (key->code ==
                    sf::Keyboard::Key::Z)
                {
                    zoom *= 1.1f;
                }

                if (key->code ==
                    sf::Keyboard::Key::X)
                {
                    zoom /= 1.1f;

                    zoom =
                        std::max(zoom, 0.1f);
                }
            }
        }

        // =====================================================
        // DELTA TIME
        // =====================================================

        float dt =
            frameClock.restart().asSeconds();

        dt = std::min(dt, 0.02f);

        // =====================================================
        // PHYSICS
        // =====================================================

        sf::Clock physicsClock;

        if (!paused)
        {
            for (auto& p : particles)
            {
                p.previousPosition =
                    p.position;

                sf::Vector2f direction =
                    blackHole - p.position;

                float distance =
                    length(direction);

                if (distance <
                    EVENT_HORIZON)
                {
                    p =
                        createParticle(
                            blackHole
                        );

                    continue;
                }

                float acceleration =
                    G * BLACK_HOLE_MASS /
                    (distance * distance);

                sf::Vector2f normalized =
                    direction / distance;

                p.velocity +=
                    normalized *
                    acceleration *
                    dt;

                p.position +=
                    p.velocity *
                    dt;

                if (
                    p.position.x < -200.0f ||
                    p.position.x >
                        WIDTH + 200.0f ||
                    p.position.y < -200.0f ||
                    p.position.y >
                        HEIGHT + 200.0f
                )
                {
                    p =
                        createParticle(
                            blackHole
                        );
                }
            }
        }

        physicsMs =
            physicsClock
                .getElapsedTime()
                .asMicroseconds()
                / 1000.0f;

        // =====================================================
        // BUILD PARTICLES
        // =====================================================

        particleVertices.resize(
            particles.size()
        );

        for (std::size_t i = 0;
             i < particles.size();
             ++i)
        {
            particleVertices[i].position =
                particles[i].position;

            float speed =
                length(
                    particles[i].velocity
                );

            sf::Color color =
                sf::Color::White;

            if (velocityColorsEnabled)
            {
                color =
                    velocityColor(speed);
            }
            else if (brightnessEnabled)
            {
                float brightness =
                    std::clamp(
                        speed / 250.0f,
                        0.2f,
                        1.0f
                    );

                std::uint8_t value =
                    static_cast<std::uint8_t>(
                        brightness * 255
                    );

                color =
                    sf::Color(
                        value,
                        value,
                        value
                    );
            }

            particleVertices[i].color =
                color;
        }

        // =====================================================
        // TRAILS
        // =====================================================

        trailVertices.clear();

        if (trailsEnabled)
        {
            trailVertices.resize(
                particles.size() * 2
            );

            for (std::size_t i = 0;
                 i < particles.size();
                 ++i)
            {
                trailVertices[i * 2].position =
                    particles[i].previousPosition;

                trailVertices[i * 2 + 1].position =
                    particles[i].position;

                trailVertices[i * 2].color =
                    sf::Color(
                        100,
                        100,
                        100,
                        60
                    );

                trailVertices[i * 2 + 1].color =
                    sf::Color(
                        255,
                        255,
                        255,
                        100
                    );
            }
        }

        // =====================================================
        // RENDER
        // =====================================================

        sf::Clock renderClock;

        window.clear(
            sf::Color(
                2,
                2,
                8
            )
        );

        // Trails
        if (trailsEnabled)
            window.draw(trailVertices);

        // Accretion disk
        if (diskEnabled)
        {
            sf::CircleShape disk(
                130.0f
            );

            disk.setOrigin(
                {130.0f, 130.0f}
            );

            disk.setPosition(
                blackHole
            );

            disk.setScale(
                {1.0f, 0.25f}
            );

            disk.setFillColor(
                sf::Color(
                    255,
                    100,
                    20,
                    25
                )
            );

            window.draw(disk);
        }

        // Glow
        if (glowEnabled)
        {
            for (int radius = 80;
                 radius > 20;
                 radius -= 10)
            {
                sf::CircleShape glow(
                    static_cast<float>(
                        radius
                    )
                );

                glow.setOrigin(
                    {
                        static_cast<float>(
                            radius
                        ),
                        static_cast<float>(
                            radius
                        )
                    }
                );

                glow.setPosition(
                    blackHole
                );

                std::uint8_t alpha =
                    static_cast<std::uint8_t>(
                        2 +
                        (80 - radius)
                    );

                glow.setFillColor(
                    sf::Color(
                        255,
                        100,
                        20,
                        alpha
                    )
                );

                window.draw(glow);
            }
        }

        // Particles
        window.draw(
            particleVertices
        );

        // Black hole
        window.draw(
            blackHoleShape
        );

        // HUD
        if (hudEnabled)
        {
            HudMetrics metrics;
            metrics.particleCount =
                particles.size();
            metrics.fps = fps;
            metrics.physicsMs = physicsMs;
            metrics.renderMs = renderMs;
            metrics.zoom = zoom;
            metrics.paused = paused;
            metrics.brightnessEnabled =
                brightnessEnabled;
            metrics.diskEnabled = diskEnabled;
            metrics.trailsEnabled =
                trailsEnabled;
            metrics.velocityColorsEnabled =
                velocityColorsEnabled;
            metrics.glowEnabled = glowEnabled;

            drawHud(window, hudFont, metrics);
        }

        window.display();

        renderMs =
            renderClock
                .getElapsedTime()
                .asMicroseconds()
                / 1000.0f;

        // =====================================================
        // FPS
        // =====================================================

        frames++;

        if (
            fpsClock
                .getElapsedTime()
                .asSeconds()
                >= 0.5f
        )
        {
            float elapsed =
                fpsClock
                    .restart()
                    .asSeconds();

            fps =
                static_cast<float>(
                    frames
                ) / elapsed;

            frames = 0;

            std::ostringstream title;

            title
                << "BLACK HOLE LAB | "
                << "Particles: "
                << particles.size()
                << " | FPS: "
                << std::fixed
                << std::setprecision(1)
                << fps
                << " | Physics: "
                << physicsMs
                << " ms"
                << " | Render: "
                << renderMs
                << " ms";

            if (paused)
                title << " | PAUSED";

            window.setTitle(
                title.str()
            );
        }
    }

    return 0;
}
