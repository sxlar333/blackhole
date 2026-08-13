#pragma once

#include <fstream>
#include <string>

/*
 * Runtime settings shared by every binary.
 *
 * Each program loads only the section it cares about from config.ini
 * (e.g. [nbody] for the n-body build) via loadConfig(). The values
 * override the built-in defaults.
 *
 * Command line:
 *   <binary> [--config <path>]
 */
struct Config
{
    float g = 5000.0f;
    float blackHoleMass = 1000.0f;
    float eventHorizon = 18.0f;
    float softening = 4.0f;
    float theta = 0.5f;

    int particles = 10000;
    int particlesPerStep = 1000;

    int benchmarkInitialParticles = 100000;
    int benchmarkParticlesPerLevel = 100000;
    int benchmarkLevelSeconds = 10;
    int benchmarkMaxLevels = 12;

    std::string configPath = "config.ini";
};

static std::string configTrim(const std::string& s)
{
    std::size_t start = 0;

    while (
        start < s.size() &&
        (s[start] == ' ' || s[start] == '\t')
    )
    {
        ++start;
    }

    std::size_t end = s.size();

    while (
        end > start &&
        (s[end - 1] == ' ' || s[end - 1] == '\t')
    )
    {
        --end;
    }

    return s.substr(start, end - start);
}

static std::string configLower(const std::string& s)
{
    std::string out = s;

    for (char& c : out)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    }

    return out;
}

static float configParseFloat(
    const std::string& value,
    float fallback
)
{
    try
    {
        return std::stof(configTrim(value));
    }
    catch (...)
    {
        return fallback;
    }
}

static int configParseInt(
    const std::string& value,
    int fallback
)
{
    try
    {
        return std::stoi(configTrim(value));
    }
    catch (...)
    {
        return fallback;
    }
}

static void configApply(
    Config& config,
    const std::string& key,
    const std::string& value
)
{
    if (key == "g")
    {
        config.g = configParseFloat(value, config.g);
    }
    else if (key == "black_hole_mass")
    {
        config.blackHoleMass =
            configParseFloat(value, config.blackHoleMass);
    }
    else if (key == "event_horizon")
    {
        config.eventHorizon =
            configParseFloat(value, config.eventHorizon);
    }
    else if (key == "softening")
    {
        config.softening =
            configParseFloat(value, config.softening);
    }
    else if (key == "theta")
    {
        config.theta = configParseFloat(value, config.theta);
    }
    else if (key == "particles")
    {
        config.particles =
            configParseInt(value, config.particles);
    }
    else if (key == "particles_per_step")
    {
        config.particlesPerStep =
            configParseInt(value, config.particlesPerStep);
    }
    else if (key == "initial_particles")
    {
        config.benchmarkInitialParticles =
            configParseInt(value, config.benchmarkInitialParticles);
    }
    else if (key == "particles_per_level")
    {
        config.benchmarkParticlesPerLevel =
            configParseInt(value, config.benchmarkParticlesPerLevel);
    }
    else if (key == "level_seconds")
    {
        config.benchmarkLevelSeconds =
            configParseInt(value, config.benchmarkLevelSeconds);
    }
    else if (key == "max_levels")
    {
        config.benchmarkMaxLevels =
            configParseInt(value, config.benchmarkMaxLevels);
    }
}

/*
 * Load settings for `section` from the config file.
 *
 * The `--config <path>` argument overrides the default "config.ini".
 * Missing files or keys silently fall back to the built-in defaults.
 */
static Config loadConfig(
    int argc,
    char** argv,
    const char* section
)
{
    std::string path = "config.ini";

    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::string(argv[i]) == "--config")
            path = argv[i + 1];
    }

    Config config;
    config.configPath = path;

    std::ifstream file(path);

    if (!file.is_open())
        return config;

    std::string currentSection;
    std::string line;

    while (std::getline(file, line))
    {
        std::string trimmed = configTrim(line);

        if (
            trimmed.empty() ||
            trimmed[0] == ';' ||
            trimmed[0] == '#'
        )
        {
            continue;
        }

        if (
            trimmed.front() == '[' &&
            trimmed.back() == ']'
        )
        {
            currentSection = configLower(
                configTrim(
                    trimmed.substr(
                        1,
                        trimmed.size() - 2
                    )
                )
            );

            continue;
        }

        std::size_t equals = trimmed.find('=');

        if (equals == std::string::npos)
            continue;

        if (configLower(currentSection) != configLower(section))
            continue;

        std::string key = configLower(
            configTrim(trimmed.substr(0, equals))
        );

        std::string value = configTrim(
            trimmed.substr(equals + 1)
        );

        configApply(config, key, value);
    }

    return config;
}
