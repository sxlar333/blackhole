#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    bool alive = true;
};

struct Quad
{
    float x;
    float y;
    float size;

    bool contains(sf::Vector2f p) const
    {
        return
            p.x >= x &&
            p.x < x + size &&
            p.y >= y &&
            p.y < y + size;
    }

    Quad nw() const
    {
        return {x, y, size / 2.0f};
    }

    Quad ne() const
    {
        return {x + size / 2.0f, y, size / 2.0f};
    }

    Quad sw() const
    {
        return {x, y + size / 2.0f, size / 2.0f};
    }

    Quad se() const
    {
        return {
            x + size / 2.0f,
            y + size / 2.0f,
            size / 2.0f
        };
    }
};

struct Node
{
    Quad boundary;

    float mass = 0.0f;
    sf::Vector2f centerOfMass{0.0f, 0.0f};

    int particle = -1;

    std::unique_ptr<Node> nw;
    std::unique_ptr<Node> ne;
    std::unique_ptr<Node> sw;
    std::unique_ptr<Node> se;

    Node(Quad q)
        : boundary(q)
    {
    }

    bool isLeaf() const
    {
        return
            !nw &&
            !ne &&
            !sw &&
            !se;
    }
};

constexpr unsigned WIDTH = 1280;
constexpr unsigned HEIGHT = 720;

constexpr float G = 80.0f;
constexpr float BLACK_HOLE_MASS = 50000.0f;
constexpr float EVENT_HORIZON = 20.0f;
constexpr float SOFTENING = 4.0f;

// Barnes-Hut accuracy parameter.
float theta = 0.5f;

constexpr int PARTICLES_PER_STEP = 1000;

float randomFloat(float min, float max)
{
    return min +
        static_cast<float>(std::rand()) /
        static_cast<float>(RAND_MAX) *
        (max - min);
}

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

float length(sf::Vector2f v)
{
    return std::sqrt(lengthSquared(v));
}

Particle createParticle(sf::Vector2f center)
{
    float angle =
        randomFloat(0.0f, 6.283185307f);

    float radius =
        randomFloat(80.0f, 330.0f);

    Particle p;

    p.position =
    {
        center.x +
            std::cos(angle) * radius,

        center.y +
            std::sin(angle) * radius
    };

    float orbitalSpeed =
        std::sqrt(
            G *
            BLACK_HOLE_MASS /
            radius
        );

    float multiplier =
        randomFloat(0.75f, 1.20f);

    p.velocity =
    {
        -std::sin(angle) *
            orbitalSpeed *
            multiplier,

        std::cos(angle) *
            orbitalSpeed *
            multiplier
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

/*
 * Insert a particle into the quadtree.
 */
void insertParticle(
    Node& node,
    int particleIndex,
    const std::vector<Particle>& particles
)
{
    const sf::Vector2f position =
        particles[particleIndex].position;

    // Update mass and center of mass.
    float oldMass = node.mass;

    node.mass += 1.0f;

    if (node.mass == 1.0f)
    {
        node.centerOfMass = position;
    }
    else
    {
        node.centerOfMass =
            (
                node.centerOfMass *
                oldMass +
                position
            ) /
            node.mass;
    }

    // Empty leaf.
    if (
        node.isLeaf() &&
        node.particle == -1
    )
    {
        node.particle =
            particleIndex;

        return;
    }

    // If this is a leaf containing a particle,
    // subdivide it.
    if (node.isLeaf())
    {
        int oldParticle =
            node.particle;

        node.particle = -1;

        node.nw =
            std::make_unique<Node>(
                node.boundary.nw()
            );

        node.ne =
            std::make_unique<Node>(
                node.boundary.ne()
            );

        node.sw =
            std::make_unique<Node>(
                node.boundary.sw()
            );

        node.se =
            std::make_unique<Node>(
                node.boundary.se()
            );

        // Reinsert old particle.
        insertParticle(
            *(
                node.nw->boundary.contains(
                    particles[oldParticle].position
                )
                    ? node.nw
                    : node.ne->boundary.contains(
                        particles[oldParticle].position
                    )
                        ? node.ne
                        : node.sw->boundary.contains(
                            particles[oldParticle].position
                        )
                            ? node.sw
                            : node.se
            ),
            oldParticle,
            particles
        );
    }

    // Insert new particle into the correct quadrant.
    if (
        node.nw->boundary.contains(position)
    )
    {
        insertParticle(
            *node.nw,
            particleIndex,
            particles
        );
    }
    else if (
        node.ne->boundary.contains(position)
    )
    {
        insertParticle(
            *node.ne,
            particleIndex,
            particles
        );
    }
    else if (
        node.sw->boundary.contains(position)
    )
    {
        insertParticle(
            *node.sw,
            particleIndex,
            particles
        );
    }
    else if (
        node.se->boundary.contains(position)
    )
    {
        insertParticle(
            *node.se,
            particleIndex,
            particles
        );
    }
}

/*
 * Calculate gravitational acceleration from the tree.
 */
sf::Vector2f calculateForce(
    const Node& node,
    int particleIndex,
    const std::vector<Particle>& particles
)
{
    if (
        node.mass == 0.0f
    )
    {
        return {0.0f, 0.0f};
    }

    const sf::Vector2f position =
        particles[particleIndex].position;

    sf::Vector2f direction =
        node.centerOfMass -
        position;

    float distance =
        length(direction);

    if (distance < SOFTENING)
    {
        return {0.0f, 0.0f};
    }

    /*
     * If this node is sufficiently far away,
     * treat the entire region as one mass.
     */
    if (
        node.isLeaf() ||
        (
            node.boundary.size /
            distance
        ) < theta
    )
    {
        float distanceSq =
            distance * distance +
            SOFTENING * SOFTENING;

        float acceleration =
            G *
            node.mass /
            distanceSq;

        return
            (
                direction /
                distance
            ) *
            acceleration;
    }

    sf::Vector2f acceleration{
        0.0f,
        0.0f
    };

    if (node.nw)
    {
        acceleration +=
            calculateForce(
                *node.nw,
                particleIndex,
                particles
            );
    }

    if (node.ne)
    {
        acceleration +=
            calculateForce(
                *node.ne,
                particleIndex,
                particles
            );
    }

    if (node.sw)
    {
        acceleration +=
            calculateForce(
                *node.sw,
                particleIndex,
                particles
            );
    }

    if (node.se)
    {
        acceleration +=
            calculateForce(
                *node.se,
                particleIndex,
                particles
            );
    }

    return acceleration;
}

int countNodes(const Node& node)
{
    int count = 1;

    if (node.nw)
        count += countNodes(*node.nw);

    if (node.ne)
        count += countNodes(*node.ne);

    if (node.sw)
        count += countNodes(*node.sw);

    if (node.se)
        count += countNodes(*node.se);

    return count;
}

int main()
{
    std::srand(
        static_cast<unsigned>(
            std::time(nullptr)
        )
    );

    sf::RenderWindow window(
        sf::VideoMode({WIDTH, HEIGHT}),
        "BLACK HOLE LAB | BARNES-HUT"
    );

    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(0);

    const sf::Vector2f blackHole{
        WIDTH / 2.0f,
        HEIGHT / 2.0f
    };

    std::vector<Particle> particles;

    addParticles(
        particles,
        10000,
        blackHole
    );

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

    sf::VertexArray vertices(
        sf::PrimitiveType::Points
    );

    sf::Clock frameClock;
    sf::Clock fpsClock;

    float fps = 0.0f;
    float physicsMs = 0.0f;
    float renderMs = 0.0f;

    int frames = 0;
    int treeNodes = 0;

    bool paused = false;

    while (window.isOpen())
    {
        // =====================================================
        // EVENTS
        // =====================================================

        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (
                auto* key =
                    event->getIf<
                        sf::Event::KeyPressed
                    >()
            )
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
                        particles.size() -
                        remove
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
                        10000,
                        blackHole
                    );
                }

                // Increase accuracy.
                if (
                    key->code ==
                    sf::Keyboard::Key::Up
                )
                {
                    theta =
                        std::max(
                            0.1f,
                            theta - 0.1f
                        );
                }

                // Increase speed.
                if (
                    key->code ==
                    sf::Keyboard::Key::Down
                )
                {
                    theta += 0.1f;
                }
            }
        }

        // =====================================================
        // DELTA TIME
        // =====================================================

        float dt =
            frameClock
                .restart()
                .asSeconds();

        dt =
            std::min(
                dt,
                0.02f
            );

        // =====================================================
        // BUILD TREE + PHYSICS
        // =====================================================

        sf::Clock physicsClock;

        if (!paused)
        {
            /*
             * Root covers the whole simulation area.
             */
            auto root =
                std::make_unique<Node>(
                    Quad{
                        0.0f,
                        0.0f,
                        static_cast<float>(
                            std::max(
                                WIDTH,
                                HEIGHT
                            )
                        )
                    }
                );

            for (
                std::size_t i = 0;
                i < particles.size();
                ++i
            )
            {
                if (!particles[i].alive)
                    continue;

                insertParticle(
                    *root,
                    static_cast<int>(i),
                    particles
                );
            }

            treeNodes =
                countNodes(*root);

            /*
             * Calculate forces.
             */
            for (
                std::size_t i = 0;
                i < particles.size();
                ++i
            )
            {
                if (!particles[i].alive)
                    continue;

                sf::Vector2f acceleration{
                    0.0f,
                    0.0f
                };

                // Black hole.
                sf::Vector2f toBlackHole =
                    blackHole -
                    particles[i].position;

                float distance =
                    length(
                        toBlackHole
                    );

                if (
                    distance <
                    EVENT_HORIZON
                )
                {
                    particles[i].alive =
                        false;

                    continue;
                }

                float distanceSq =
                    distance *
                    distance +
                    SOFTENING *
                    SOFTENING;

                float blackHoleAcceleration =
                    G *
                    BLACK_HOLE_MASS /
                    distanceSq;

                acceleration +=
                    (
                        toBlackHole /
                        distance
                    ) *
                    blackHoleAcceleration;

                // Other particles.
                acceleration +=
                    calculateForce(
                        *root,
                        static_cast<int>(i),
                        particles
                    );

                particles[i].velocity +=
                    acceleration *
                    dt;

                particles[i].position +=
                    particles[i].velocity *
                    dt;
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
        // RENDER
        // =====================================================

        vertices.resize(
            particles.size()
        );

        for (
            std::size_t i = 0;
            i < particles.size();
            ++i
        )
        {
            vertices[i].position =
                particles[i].position;

            vertices[i].color =
                sf::Color::White;
        }

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
                << "BARNES-HUT | "
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
                << " ms"
                << " | Theta: "
                << std::setprecision(2)
                << theta
                << " | Nodes: "
                << treeNodes;

            if (paused)
                title << " | PAUSED";

            window.setTitle(
                title.str()
            );
        }
    }

    return 0;
}
