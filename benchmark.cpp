#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <string>

#include "config.h"

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    float speed;
};

constexpr unsigned int WIDTH = 1280;
constexpr unsigned int HEIGHT = 720;

float G = 5000.0f;
float BLACK_HOLE_MASS = 1000.0f;
float EVENT_HORIZON = 18.0f;

struct BenchmarkConfig
{
    int initialParticles = 100000;
    int particlesPerLevel = 100000;
    float levelSeconds = 10.0f;
    int maxLevels = 12;

    float maxDuration() const
    {
        return levelSeconds * static_cast<float>(maxLevels);
    }
};

constexpr int FIELD_COUNT = 4;

struct SetupState
{
    std::string levels;
    std::string levelSeconds;
    std::string increment;
    std::string initial;
    int selected = 0;
};

const std::string& fieldInput(const SetupState& state, int index)
{
    switch (index)
    {
        case 0: return state.levels;
        case 1: return state.levelSeconds;
        case 2: return state.increment;
        default: return state.initial;
    }
}

std::string& fieldInput(SetupState& state, int index)
{
    switch (index)
    {
        case 0: return state.levels;
        case 1: return state.levelSeconds;
        case 2: return state.increment;
        default: return state.initial;
    }
}

int parseCount(
    const std::string& input,
    int fallback,
    int minValue
)
{
    if (input.empty())
        return fallback;

    try
    {
        long long value = std::stoll(input);

        value =
            std::max(
                value,
                static_cast<long long>(minValue)
            );

        value =
            std::min(
                value,
                1000000000LL
            );

        return static_cast<int>(value);
    }
    catch (...)
    {
        return fallback;
    }
}

float parseSeconds(
    const std::string& input,
    float fallback
)
{
    if (input.empty())
        return fallback;

    try
    {
        long long value = std::stoll(input);

        value =
            std::max(
                value,
                1LL
            );

        return static_cast<float>(value);
    }
    catch (...)
    {
        return fallback;
    }
}

BenchmarkConfig parseSetup(const SetupState& state)
{
    BenchmarkConfig config;

    config.maxLevels =
        parseCount(state.levels, config.maxLevels, 1);

    config.levelSeconds =
        parseSeconds(
            state.levelSeconds,
            config.levelSeconds
        );

    config.particlesPerLevel =
        parseCount(
            state.increment,
            config.particlesPerLevel,
            0
        );

    config.initialParticles =
        parseCount(
            state.initial,
            config.initialParticles,
            0
        );

    return config;
}

unsigned int rngState = 0;

float randomFloat(float min, float max)
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;

    float t =
        static_cast<float>(rngState) / 4294967295.0f;

    return min + t * (max - min);
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

    p.speed = orbitalSpeed * randomness;

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

struct LevelResult
{
    std::size_t particles;
    float seconds;
    float avgFps;
    float minFps;
    float avgPhysicsMs;
    float avgRenderMs;
};

std::string formatSeconds(float s)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << s;
    return out.str();
}

std::string formatFps(float fps)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << fps;
    return out.str();
}

std::string formatMs(float ms)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << ms;
    return out.str();
}

void drawSetupScreen(
    sf::RenderWindow& window,
    const sf::Font& font,
    const SetupState& state,
    const BenchmarkConfig& defaults
)
{
    const BenchmarkConfig preview = parseSetup(state);

    static const char* labels[FIELD_COUNT] = {
        "levels",
        "level length",
        "increment",
        "initial particles"
    };

    static const char* suffixes[FIELD_COUNT] = {
        "",
        " s",
        "",
        ""
    };

    const float startY = 200.0f;
    const float lineHeight = 36.0f;
    const float labelX = 440.0f;
    const float valueX = 650.0f;

    sf::Text title(
        font,
        "BENCHMARK SETUP",
        26
    );

    title.setFillColor(
        sf::Color(255, 166, 64)
    );

    title.setPosition(
        {labelX - 40.0f, 140.0f}
    );

    window.draw(title);

    for (int i = 0; i < FIELD_COUNT; ++i)
    {
        const std::string& input =
            fieldInput(state, i);

        std::string value;
        bool editing = !input.empty();

        if (editing)
        {
            value = input;
        }
        else
        {
            switch (i)
            {
                case 0:
                    value =
                        std::to_string(
                            defaults.maxLevels
                        );
                    break;

                case 1:
                    value =
                        std::to_string(
                            static_cast<int>(
                                defaults.levelSeconds
                            )
                        );
                    break;

                case 2:
                    value =
                        std::to_string(
                            defaults.particlesPerLevel
                        );
                    break;

                default:
                    value =
                        std::to_string(
                            defaults.initialParticles
                        );
                    break;
            }
        }

        value += suffixes[i];

        sf::Text label(
            font,
            labels[i],
            16
        );

        sf::Text valueText(
            font,
            value,
            16
        );

        if (i == state.selected)
        {
            label.setFillColor(
                sf::Color(255, 166, 64)
            );

            valueText.setFillColor(
                sf::Color(255, 220, 170)
            );
        }
        else
        {
            label.setFillColor(
                sf::Color(155, 158, 168)
            );

            valueText.setFillColor(
                editing
                    ? sf::Color(225, 225, 230)
                    : sf::Color(120, 122, 132)
            );
        }

        label.setPosition(
            {labelX, startY + i * lineHeight}
        );

        valueText.setPosition(
            {valueX, startY + i * lineHeight}
        );

        window.draw(label);
        window.draw(valueText);
    }

    std::ostringstream hints;
    hints
        << "UP / DOWN       select field\n"
        << "0-9             type a value  (leave empty = default)\n"
        << "BACKSPACE       delete last digit\n"
        << "\n"
        << "estimated total "
        << formatSeconds(preview.maxDuration())
        << " s\n"
        << "\n"
        << "ENTER           start benchmark\n"
        << "ESC             quit";

    sf::Text hintText(
        font,
        hints.str(),
        13
    );

    hintText.setFillColor(
        sf::Color(120, 122, 132)
    );

    hintText.setPosition(
        {
            labelX - 40.0f,
            startY + FIELD_COUNT * lineHeight + 26.0f
        }
    );

    window.draw(hintText);
}

void drawPanel(
    sf::RenderWindow& window,
    const sf::Font& font,
    sf::Vector2f position,
    const std::string& content,
    unsigned int characterSize,
    const sf::Color& textColor
)
{
    sf::Text text(
        font,
        content,
        characterSize
    );

    text.setFillColor(textColor);
    text.setPosition(position);

    window.draw(text);
}

void drawLiveHud(
    sf::RenderWindow& window,
    const sf::Font& font,
    int level,
    int maxLevels,
    std::size_t particleCount,
    float levelSeconds,
    float totalSeconds,
    float fps,
    float physicsMs,
    float renderMs,
    float levelDuration,
    float maxDuration
)
{
    std::ostringstream hud;
    hud
        << "BLACK HOLE BENCHMARK\n"
        << "\n"
        << "level      "
        << level
        << " / "
        << maxLevels
        << "\n"
        << "particles  "
        << particleCount
        << "\n"
        << "level time "
        << formatSeconds(levelSeconds)
        << "s / "
        << formatSeconds(levelDuration)
        << "s\n"
        << "total time "
        << formatSeconds(totalSeconds)
        << "s / "
        << formatSeconds(maxDuration)
        << "s\n"
        << "fps        "
        << formatFps(fps)
        << "\n"
        << "physics    "
        << formatMs(physicsMs)
        << " ms\n"
        << "render     "
        << formatMs(renderMs)
        << " ms\n"
        << "\n"
        << "[ESC] end benchmark";

    drawPanel(
        window,
        font,
        {16.0f, 16.0f},
        hud.str(),
        15,
        sf::Color(225, 225, 230)
    );
}

void drawResultsPopup(
    sf::RenderWindow& window,
    const sf::Font& font,
    const std::vector<LevelResult>& results
)
{
    if (results.empty())
        return;

    std::ostringstream table;

    table
        << std::left
        << std::setw(11) << "particles"
        << std::setw(9) << "avg fps"
        << std::setw(9) << "min fps"
        << std::setw(11) << "physics ms"
        << std::setw(10) << "render ms"
        << "\n";

    std::size_t totalFrames = 0;
    float totalSeconds = 0.0f;

    for (const auto& r : results)
    {
        table
            << std::left
            << std::setw(11) << r.particles
            << std::setw(9) << std::fixed << std::setprecision(1) << r.avgFps
            << std::setw(9) << r.minFps
            << std::setw(11) << std::setprecision(2) << r.avgPhysicsMs
            << std::setw(10) << r.avgRenderMs
            << "\n";

        totalFrames += static_cast<std::size_t>(
            r.avgFps * r.seconds
        );
        totalSeconds += r.seconds;
    }

    float overallFps =
        totalSeconds > 0.0f
            ? static_cast<float>(totalFrames) / totalSeconds
            : 0.0f;

    const LevelResult* best = &results.front();
    const LevelResult* worst = &results.front();

    for (const auto& r : results)
    {
        if (r.avgFps > best->avgFps)
            best = &r;

        if (r.avgFps < worst->avgFps)
            worst = &r;
    }

    std::ostringstream summary;
    summary
        << "levels       "
        << results.size()
        << "\n"
        << "total time   "
        << formatSeconds(totalSeconds)
        << " s\n"
        << "avg fps      "
        << formatFps(overallFps)
        << "\n"
        << "best level   "
        << formatFps(best->avgFps)
        << " fps at "
        << best->particles
        << " particles\n"
        << "worst level  "
        << formatFps(worst->avgFps)
        << " fps at "
        << worst->particles
        << " particles";

    const std::string hint =
        "press ESC to close";

    const float contentMargin = 20.0f;

    sf::Text titleText(
        font,
        "BENCHMARK COMPLETE",
        24
    );

    sf::Text tableText(
        font,
        table.str(),
        14
    );

    sf::Text summaryText(
        font,
        summary.str(),
        14
    );

    sf::Text hintText(
        font,
        hint,
        13
    );

    titleText.setFillColor(
        sf::Color(255, 166, 64)
    );

    tableText.setFillColor(
        sf::Color(225, 225, 230)
    );

    summaryText.setFillColor(
        sf::Color(155, 158, 168)
    );

    hintText.setFillColor(
        sf::Color(120, 122, 132)
    );

    const float titleWidth =
        titleText.getLocalBounds().size.x;
    const float tableWidth =
        tableText.getLocalBounds().size.x;
    const float summaryWidth =
        summaryText.getLocalBounds().size.x;
    const float hintWidth =
        hintText.getLocalBounds().size.x;

    float panelWidth =
        std::max(
            titleWidth,
            std::max(
                tableWidth,
                std::max(summaryWidth, hintWidth)
            )
        ) +
        contentMargin * 2.0f;

    float panelHeight =
        contentMargin * 2.0f +
        30.0f +
        tableText.getLocalBounds().size.y +
        16.0f +
        summaryText.getLocalBounds().size.y +
        20.0f +
        hintText.getLocalBounds().size.y;

    panelWidth =
        std::max(panelWidth, 520.0f);

    panelHeight =
        std::max(panelHeight, 320.0f);

    sf::RectangleShape overlay(
        {
            static_cast<float>(WIDTH),
            static_cast<float>(HEIGHT)
        }
    );

    overlay.setFillColor(
        sf::Color(0, 0, 0, 170)
    );

    window.draw(overlay);

    sf::RectangleShape panel(
        {panelWidth, panelHeight}
    );

    panel.setPosition(
        {
            (WIDTH - panelWidth) / 2.0f,
            (HEIGHT - panelHeight) / 2.0f
        }
    );

    panel.setFillColor(
        sf::Color(10, 10, 20, 240)
    );

    panel.setOutlineThickness(
        1.0f
    );

    panel.setOutlineColor(
        sf::Color(255, 166, 64, 80)
    );

    window.draw(panel);

    sf::Vector2f origin =
        panel.getPosition();

    sf::Vector2f contentX(
        origin.x + contentMargin,
        0.0f
    );

    float y = origin.y + contentMargin;

    titleText.setPosition(
        {contentX.x, y}
    );

    window.draw(titleText);

    y += 30.0f;

    tableText.setPosition(
        {contentX.x, y}
    );

    window.draw(tableText);

    y +=
        tableText.getLocalBounds().size.y +
        16.0f;

    summaryText.setPosition(
        {contentX.x, y}
    );

    window.draw(summaryText);

    y +=
        summaryText.getLocalBounds().size.y +
        20.0f;

    hintText.setPosition(
        {contentX.x, y}
    );

    window.draw(hintText);
}

int main(int argc, char** argv)
{
    rngState =
        static_cast<unsigned>(
            std::time(nullptr)
        ) | 1u;

    const Config fileConfig =
        loadConfig(argc, argv, "benchmark");

    G = fileConfig.g;
    BLACK_HOLE_MASS = fileConfig.blackHoleMass;
    EVENT_HORIZON = fileConfig.eventHorizon;

    BenchmarkConfig setupDefaults;

    setupDefaults.initialParticles =
        fileConfig.benchmarkInitialParticles;

    setupDefaults.particlesPerLevel =
        fileConfig.benchmarkParticlesPerLevel;

    setupDefaults.levelSeconds =
        static_cast<float>(
            fileConfig.benchmarkLevelSeconds
        );

    setupDefaults.maxLevels =
        fileConfig.benchmarkMaxLevels;

    sf::RenderWindow window(
        sf::VideoMode({WIDTH, HEIGHT}),
        "BLACK HOLE BENCHMARK"
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

    SetupState setup;

    bool start = false;
    bool quit = false;

    while (window.isOpen())
    {
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code ==
                    sf::Keyboard::Key::Up)
                {
                    setup.selected =
                        (setup.selected +
                         FIELD_COUNT - 1) %
                        FIELD_COUNT;
                }
                else if (key->code ==
                    sf::Keyboard::Key::Down)
                {
                    setup.selected =
                        (setup.selected + 1) %
                        FIELD_COUNT;
                }
                else if (key->code ==
                    sf::Keyboard::Key::Backspace)
                {
                    std::string& input =
                        fieldInput(
                            setup,
                            setup.selected
                        );

                    if (!input.empty())
                        input.pop_back();
                }
                else if (key->code ==
                    sf::Keyboard::Key::Enter)
                {
                    start = true;
                }
                else if (key->code ==
                    sf::Keyboard::Key::Escape)
                {
                    quit = true;
                }
            }

            if (auto* text =
                event->getIf<sf::Event::TextEntered>())
            {
                if (text->unicode >= U'0' &&
                    text->unicode <= U'9')
                {
                    std::string& input =
                        fieldInput(
                            setup,
                            setup.selected
                        );

                    input +=
                        static_cast<char>(
                            text->unicode
                        );
                }
            }
        }

        if (start || quit)
            break;

        window.clear(
            sf::Color(2, 2, 8)
        );

        drawSetupScreen(
            window,
            hudFont,
            setup,
            setupDefaults
        );

        window.display();
    }

    if (quit || !window.isOpen())
        return 0;

    const BenchmarkConfig config =
        parseSetup(setup);

    sf::Vector2f blackHole(
        WIDTH / 2.0f,
        HEIGHT / 2.0f
    );

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

    std::vector<Particle> particles;

    addParticles(
        particles,
        config.initialParticles,
        blackHole
    );

    sf::VertexArray particleVertices(
        sf::PrimitiveType::Points
    );

    sf::Clock frameClock;
    sf::Clock levelClock;
    sf::Clock benchmarkClock;
    sf::Clock fpsClock;

    const int maxLevels = config.maxLevels;

    int level = 1;
    int frames = 0;
    int fpsFrames = 0;

    float minFps =
        std::numeric_limits<float>::max();

    double physicsSumMs = 0.0;
    double renderSumMs = 0.0;

    float fps = 0.0f;
    float physicsMs = 0.0f;
    float renderMs = 0.0f;

    std::vector<LevelResult> results;

    bool finished = false;

    auto finishLevel =
        [&]()
        {
            if (frames <= 0)
                return;

            float seconds =
                levelClock
                    .getElapsedTime()
                    .asSeconds();

            LevelResult r;

            r.particles = particles.size();
            r.seconds = seconds;
            r.avgFps =
                static_cast<float>(frames) / seconds;
            r.minFps =
                minFps ==
                    std::numeric_limits<float>::max()
                    ? r.avgFps
                    : minFps;
            r.avgPhysicsMs =
                static_cast<float>(physicsSumMs / frames);
            r.avgRenderMs =
                static_cast<float>(renderSumMs / frames);

            results.push_back(r);

            frames = 0;
            fpsFrames = 0;

            minFps =
                std::numeric_limits<float>::max();

            physicsSumMs = 0.0;
            renderSumMs = 0.0;
        };

    auto startNextLevel =
        [&]()
        {
            addParticles(
                particles,
                config.particlesPerLevel,
                blackHole
            );

            level++;

            levelClock.restart();
        };

    while (window.isOpen())
    {
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
                    finished = true;
                }
            }
        }

        if (finished)
        {
            finishLevel();
            break;
        }

        float dt =
            frameClock.restart().asSeconds();

        dt = std::min(dt, 0.02f);

        sf::Clock physicsClock;

        for (auto& p : particles)
        {
            sf::Vector2f direction =
                blackHole - p.position;

            float distanceSq =
                direction.x * direction.x +
                direction.y * direction.y;

            if (distanceSq <
                EVENT_HORIZON * EVENT_HORIZON)
            {
                p =
                    createParticle(
                        blackHole
                    );

                continue;
            }

            float acceleration =
                G * BLACK_HOLE_MASS /
                distanceSq;

            p.velocity +=
                direction *
                (acceleration * dt /
                 std::sqrt(distanceSq));

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

                continue;
            }

            p.speed =
                std::sqrt(
                    p.velocity.x * p.velocity.x +
                    p.velocity.y * p.velocity.y
                );
        }

        physicsMs =
            physicsClock
                .getElapsedTime()
                .asMicroseconds()
                / 1000.0f;

        particleVertices.resize(
            particles.size()
        );

        for (std::size_t i = 0;
             i < particles.size();
             ++i)
        {
            particleVertices[i].position =
                particles[i].position;

            float brightness =
                std::clamp(
                    particles[i].speed / 250.0f,
                    0.2f,
                    1.0f
                );

            std::uint8_t value =
                static_cast<std::uint8_t>(
                    brightness * 255
                );

            particleVertices[i].color =
                sf::Color(
                    value,
                    value,
                    value
                );
        }

        sf::Clock renderClock;

        window.clear(
            sf::Color(
                2,
                2,
                8
            )
        );

        window.draw(
            particleVertices
        );

        window.draw(
            blackHoleShape
        );

        drawLiveHud(
            window,
            hudFont,
            level,
            maxLevels,
            particles.size(),
            levelClock.getElapsedTime().asSeconds(),
            benchmarkClock.getElapsedTime().asSeconds(),
            fps,
            physicsMs,
            renderMs,
            config.levelSeconds,
            config.maxDuration()
        );

        window.display();

        renderMs =
            renderClock
                .getElapsedTime()
                .asMicroseconds()
                / 1000.0f;

        frames++;
        fpsFrames++;

        physicsSumMs += physicsMs;
        renderSumMs += renderMs;

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
                    fpsFrames
                ) / elapsed;

            minFps =
                std::min(minFps, fps);

            fpsFrames = 0;
        }

        if (
            benchmarkClock
                .getElapsedTime()
                .asSeconds()
                >= config.maxDuration()
        )
        {
            finishLevel();
            finished = true;
        }
        else if (
            levelClock
                .getElapsedTime()
                .asSeconds()
                >= config.levelSeconds
        )
        {
            finishLevel();
            startNextLevel();
        }
    }

    while (window.isOpen())
    {
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                if (
                    key->code ==
                        sf::Keyboard::Key::Escape ||
                    key->code ==
                        sf::Keyboard::Key::Enter
                )
                {
                    window.close();
                }
            }
        }

        window.clear(
            sf::Color(2, 2, 8)
        );

        drawResultsPopup(
            window,
            hudFont,
            results
        );

        window.display();
    }

    return 0;
}
