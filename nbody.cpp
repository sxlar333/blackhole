#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

#include "config.h"

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    bool alive = true;
};

constexpr unsigned WIDTH = 1280;
constexpr unsigned HEIGHT = 720;

float G = 80.0f;
float SOFTENING = 4.0f;
float BLACK_HOLE_MASS = 50000.0f;
float EVENT_HORIZON = 20.0f;

int PARTICLES_PER_STEP = 1000;

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

float length(sf::Vector2f v)
{
    return std::sqrt(lengthSquared(v));
}

float randomFloat(float min, float max)
{
    return min +
        static_cast<float>(std::rand()) / RAND_MAX *
        (max - min);
}

Particle createParticle(sf::Vector2f center)
{
    float angle = randomFloat(0.0f, 6.283185307f);
    float radius = randomFloat(80.0f, 330.0f);

    Particle p;

    p.position =
    {
        center.x + std::cos(angle) * radius,
        center.y + std::sin(angle) * radius
    };

    /*
     * Rough circular orbital velocity.
     *
     * This is intentionally not perfectly accurate.
     * We're making a fun N-body sandbox first.
     */
    float orbitalSpeed =
        std::sqrt(G * BLACK_HOLE_MASS / radius);

    float velocityMultiplier =
        randomFloat(0.75f, 1.20f);

    p.velocity =
    {
        -std::sin(angle) *
            orbitalSpeed *
            velocityMultiplier,

        std::cos(angle) *
            orbitalSpeed *
            velocityMultiplier
    };

    return p;
}

void addParticles(
    std::vector<Particle>& particles,
    int amount,
    sf::Vector2f center
)
{
    particles.reserve(
        particles.size() + amount
    );

    for (int i = 0; i < amount; ++i)
    {
        particles.push_back(
            createParticle(center)
        );
    }
}

int main(int argc, char** argv)
{
    std::srand(
        static_cast<unsigned>(
            std::time(nullptr)
        )
    );

    const Config config =
        loadConfig(argc, argv, "nbody");

    G = config.g;
    SOFTENING = config.softening;
    BLACK_HOLE_MASS = config.blackHoleMass;
    EVENT_HORIZON = config.eventHorizon;
    PARTICLES_PER_STEP = config.particlesPerStep;

    const int initialParticles = config.particles;

    sf::RenderWindow window(
        sf::VideoMode({WIDTH, HEIGHT}),
        "BLACK HOLE LAB | N-BODY MODE"
    );

    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(0);

    const sf::Vector2f blackHole(
        WIDTH / 2.0f,
        HEIGHT / 2.0f
    );

    std::vector<Particle> particles;

    // Start small. We can make the laptop suffer later.
    addParticles(
        particles,
        initialParticles,
        blackHole
    );

    // =========================================================
    // BLACK HOLE
    // =========================================================

    sf::CircleShape blackHoleShape(
        EVENT_HORIZON
    );

    blackHoleShape.setOrigin(
        {
            EVENT_HORIZON,
            EVENT_HORIZON
        }
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

    sf::VertexArray vertices(
        sf::PrimitiveType::Points
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

    bool paused = false;

    while (window.isOpen())
    {
        // =====================================================
        // EVENTS
        // =====================================================

        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                if (
                    key->code ==
                    sf::Keyboard::Key::Escape
                )
                {
                    window.close();
                }

                if (
                    key->code ==
                    sf::Keyboard::Key::Space
                )
                {
                    paused = !paused;
                }

                if (
                    key->code ==
                    sf::Keyboard::Key::Equal
                )
                {
                    addParticles(
                        particles,
                        PARTICLES_PER_STEP,
                        blackHole
                    );
                }

                if (
                    key->code ==
                    sf::Keyboard::Key::Hyphen
                )
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

                if (
                    key->code ==
                    sf::Keyboard::Key::R
                )
                {
                    particles.clear();

                    addParticles(
                        particles,
                        initialParticles,
                        blackHole
                    );
                }
            }
        }

        // =====================================================
        // DELTA TIME
        // =====================================================

        float dt =
            frameClock.restart()
                .asSeconds();

        // Prevent enormous jumps if the program stalls.
        dt = std::min(dt, 0.02f);

        // =====================================================
        // N-BODY PHYSICS
        // =====================================================

        sf::Clock physicsClock;

        if (!paused)
        {
            /*
             * IMPORTANT:
             *
             * This is deliberately O(n²).
             *
             * Every particle interacts with every other
             * particle.
             */

            for (std::size_t i = 0;
                 i < particles.size();
                 ++i)
            {
                if (!particles[i].alive)
                    continue;

                sf::Vector2f acceleration(
                    0.0f,
                    0.0f
                );

                // -------------------------------------------------
                // BLACK HOLE GRAVITY
                // -------------------------------------------------

                sf::Vector2f toBlackHole =
                    blackHole -
                    particles[i].position;

                float blackHoleDistance =
                    length(toBlackHole);

                if (
                    blackHoleDistance <
                    EVENT_HORIZON
                )
                {
                    particles[i].alive = false;
                    continue;
                }

                float blackHoleDistanceSq =
                    blackHoleDistance *
                    blackHoleDistance;

                float blackHoleAcceleration =
                    G *
                    BLACK_HOLE_MASS /
                    (
                        blackHoleDistanceSq +
                        SOFTENING
                    );

                acceleration +=
                    (
                        toBlackHole /
                        blackHoleDistance
                    ) *
                    blackHoleAcceleration;

                // -------------------------------------------------
                // PARTICLE ↔ PARTICLE GRAVITY
                // -------------------------------------------------

                for (std::size_t j = 0;
                     j < particles.size();
                     ++j)
                {
                    if (i == j)
                        continue;

                    if (!particles[j].alive)
                        continue;

                    sf::Vector2f direction =
                        particles[j].position -
                        particles[i].position;

                    float distanceSq =
                        lengthSquared(
                            direction
                        );

                    /*
                     * Softening prevents two particles
                     * occupying nearly the same position
                     * from producing absurd acceleration.
                     */

                    distanceSq +=
                        SOFTENING *
                        SOFTENING;

                    float inverseDistance =
                        1.0f /
                        std::sqrt(distanceSq);

                    float force =
                        G *
                        inverseDistance *
                        inverseDistance;

                    acceleration +=
                        direction *
                        (
                            force *
                            inverseDistance
                        );
                }

                // -------------------------------------------------
                // INTEGRATION
                // -------------------------------------------------

                particles[i].velocity +=
                    acceleration * dt;

                particles[i].position +=
                    particles[i].velocity * dt;
            }
        }

        physicsMs =
            physicsClock
                .getElapsedTime()
                .asMicroseconds()
                / 1000.0f;

        // =====================================================
        // REMOVE DEAD PARTICLES
        // =====================================================

        particles.erase(
            std::remove_if(
                particles.begin(),
                particles.end(),
                [](const Particle& p)
                {
                    return !p.alive;
                }
            ),
            particles.end()
        );

        // =====================================================
        // BUILD VERTICES
        // =====================================================

        vertices.resize(
            particles.size()
        );

        for (std::size_t i = 0;
             i < particles.size();
             ++i)
        {
            vertices[i].position =
                particles[i].position;

            vertices[i].color =
                sf::Color::White;
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

        window.draw(vertices);

        window.draw(
            blackHoleShape
        );

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
                << "N-BODY | "
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
