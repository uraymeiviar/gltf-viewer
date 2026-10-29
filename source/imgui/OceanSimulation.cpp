// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/OceanSimulation.h"

#include "imgui/EmbeddedVFS.h"
#include "imgui/GlCaps.h"
#include "imgui/Profiling.h"
#include "core/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ux3d::imgui {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kGravity = 9.81;
// Loop design: each cascade's lowest angular frequency is at least kQuantisation x its frequency quantum 2 pi m / T,
// and the shortest wave of a band is sampled at about kFramesPerPeriod frames per period (B = (F / 7.5)^2).
constexpr double kQuantisation = 1.25;
constexpr double kFramesPerPeriod = 6.0;
constexpr double kShortestWave = 0.04; // m, the finest simulated wavelength
constexpr float kFoamRecoveryPerSecond = 0.6f; // the accumulated Jacobian climbs back by this per second

// Deterministic per-texel random numbers (independent of the iteration order and the C++ library).
uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

double uniform01(uint32_t a, uint32_t b, uint32_t c, uint32_t salt)
{
    const uint32_t h = hash32(a * 0x9E3779B1U ^ hash32(b * 0x85EBCA77U ^ hash32(c * 0xC2B2AE3DU ^ salt)));
    return (static_cast<double>(h) + 0.5) / 4294967296.0;
}

// JONSWAP (Hasselmann et al. 1973) with the fetch-limited alpha and peak frequency.
struct Jonswap {
    double alpha = 0.0, peakOmega = 1.0, gamma = 3.3, windSpeed = 1.0, swell = 0.0;

    double spectrum(double omega) const
    {
        if (omega <= 0.0) return 0.0;
        const double sigma = omega <= peakOmega ? 0.07 : 0.09;
        const double r = std::exp(-(omega - peakOmega) * (omega - peakOmega) / (2.0 * sigma * sigma * peakOmega * peakOmega));
        return alpha * kGravity * kGravity / std::pow(omega, 5.0) * std::exp(-1.25 * std::pow(peakOmega / omega, 4.0)) *
               std::pow(gamma, r);
    }

    // Horvath 2015: Donelan-Banner / Mitsuyasu spreading exponent plus the swell term, normalised cos^2s(theta / 2).
    double spread(double theta, double omega) const
    {
        const double ratio = omega / peakOmega;
        double s = ratio <= 1.0 ? 6.97 * std::pow(ratio, 4.06)
                                : 9.77 * std::pow(ratio, -2.33 - 1.45 * (windSpeed * peakOmega / kGravity - 1.17));
        s += 16.0 * std::tanh(peakOmega / omega) * swell * swell;
        s = std::max(s, 1.0e-3);
        const double logQ = (2.0 * s - 1.0) * std::log(2.0) - std::log(kPi) + 2.0 * std::lgamma(s + 1.0) - std::lgamma(2.0 * s + 1.0);
        const double c = std::abs(std::cos(0.5 * theta));
        return c <= 0.0 ? 0.0 : std::exp(logQ + 2.0 * s * std::log(c));
    }

    // Directional wave-number spectrum S(kx, kz) (m^4): S(omega) D(theta, omega) (d omega / dk) / k.
    double waveNumberSpectrum(double k, double theta) const
    {
        const double omega = std::sqrt(kGravity * k);
        return spectrum(omega) * spread(theta, omega) * (kGravity / (2.0 * omega)) / k;
    }
};

Jonswap jonswapFor(const OceanSimulation::Key& key)
{
    Jonswap j;
    j.windSpeed = std::max(static_cast<double>(key.windSpeed), 0.5);
    // Beyond a dimensionless fetch g F / U^2 of about 2.2e4 the sea is fully developed (Pierson-Moskowitz: alpha 0.0081,
    // peak 0.86 g / U); a longer fetch adds nothing.
    const double fetch = std::clamp(static_cast<double>(key.fetch), 1000.0,
                                    std::max(2.2e4 * j.windSpeed * j.windSpeed / kGravity, 1000.0));
    j.alpha = 0.076 * std::pow(j.windSpeed * j.windSpeed / (fetch * kGravity), 0.22);
    j.peakOmega = 22.0 * std::pow(kGravity * kGravity / (j.windSpeed * fetch), 1.0 / 3.0);
    j.swell = std::clamp(static_cast<double>(key.swell), 0.0, 1.0);
    return j;
}

std::string loadSource(const char* name)
{
    const VfsFile file = EmbeddedVFS::instance().getFile(std::string("shaders/") + name);
    if (!file.isValid()) {
        core::logError("[Ocean] %s is missing from the embedded resources", name);
        return std::string();
    }
    return std::string(file.asStringView());
}

GLuint buildCompute(const char* name, int resolution)
{
    const std::string body = loadSource(name);
    if (body.empty()) return 0;
    int log2 = 0;
    while ((1 << log2) < resolution) ++log2;
    const std::string source = "#version 430\n#define OCEAN_N " + std::to_string(resolution) + "\n#define OCEAN_HALF_N " +
                               std::to_string(resolution / 2) + "\n#define OCEAN_LOG2N " +
                               std::to_string(log2) + "\n" + body;
    const char* text = source.c_str();
    const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        core::logError("[Ocean] %s does not compile: %s", name, log);
        glDeleteShader(shader);
        return 0;
    }
    profiling::countShaderCompile();
    const GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        core::logError("[Ocean] %s does not link: %s", name, log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// GL state the simulation changes, restored afterwards (the renderer and ImGui cache theirs).
struct SavedState {
    GLint program = 0, activeTexture = GL_TEXTURE0, texture2d = 0, texture2dArray = 0, unpackBuffer = 0, unpackAlignment = 4,
          unpackRowLength = 0;
    void save()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &texture2dArray);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    void restore() const
    {
        for (GLuint unit = 0; unit < 3; ++unit) {
            glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        }
        glUseProgram(static_cast<GLuint>(program));
        glActiveTexture(static_cast<GLenum>(activeTexture));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2d));
        glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(texture2dArray));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer));
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
    }
};

} // namespace

bool oceanAvailable()
{
    static const bool testNoCompute = [] {
        const char* value = std::getenv("GLIDER_TEST_NO_COMPUTE");
        return value != nullptr && value[0] == '1';
    }();
    return glCaps().compute && !testNoCompute;
}

OceanSimulation::QualityInfo OceanSimulation::qualityInfo(OceanQuality quality)
{
    switch (quality) {
    case OceanQuality::Low: return { 2, 128, 32 };
    case OceanQuality::High: return { 4, 256, 64 };
    case OceanQuality::Medium:
    default: return { 3, 256, 48 };
    }
}

OceanSimulation::Key OceanSimulation::keyOf(const OceanSettings& s)
{
    Key key;
    key.windSpeed = s.windSpeed;
    key.windDirection = s.windDirection;
    key.fetch = s.fetch;
    key.choppiness = s.choppiness;
    key.swell = s.swell;
    key.period = s.period;
    key.quality = s.quality;
    return key;
}

OceanSimulation::~OceanSimulation()
{
    release();
}

void OceanSimulation::destroy(Set& set)
{
    if (set.texture != 0) glDeleteTextures(1, &set.texture);
    set.texture = 0;
}

void OceanSimulation::releasePrograms()
{
    for (GLuint* program : { &m_spectrumProgram, &m_fftProgram, &m_finalizeProgram, &m_foamProgram, &m_mipProgram }) {
        if (*program != 0) glDeleteProgram(*program);
        *program = 0;
    }
    m_programResolution = 0;
}

void OceanSimulation::release()
{
    if (m_front.texture == 0 && !m_job.active && m_programResolution == 0 && m_spectrumTexture == 0 &&
        m_scratchTexture == 0 && m_timers[0].queries[0] == 0) {
        return;
    }
    abortJob();
    destroy(m_front);
    m_front = Set();
    releasePrograms();
    if (m_spectrumTexture != 0) glDeleteTextures(1, &m_spectrumTexture);
    if (m_scratchTexture != 0) glDeleteTextures(1, &m_scratchTexture);
    m_spectrumTexture = m_scratchTexture = 0;
    m_scratchResolution = 0;
    for (SliceTimer& timer : m_timers) {
        if (timer.queries[0] != 0) glDeleteQueries(2, timer.queries);
        timer = SliceTimer();
    }
}

float OceanSimulation::progress() const
{
    if (!m_job.active) return ready() ? 1.0f : 0.0f;
    return m_job.itemCount > 0 ? static_cast<float>(m_job.nextItem) / static_cast<float>(m_job.itemCount) : 0.0f;
}

uint64_t OceanSimulation::memoryBytes() const
{
    uint64_t bytes = m_front.bytes + (m_job.active ? m_job.set.bytes : 0);
    if (m_spectrumTexture != 0) bytes += static_cast<uint64_t>(m_scratchResolution) * m_scratchResolution * 16u * kMaxCascades;
    if (m_scratchTexture != 0) bytes += static_cast<uint64_t>(m_scratchResolution) * m_scratchResolution * 16u;
    return bytes;
}

double OceanSimulation::framePosition(int cascadeIndex, double time) const
{
    if (m_front.frames <= 0 || m_front.key.period <= 0.0f) return 0.0;
    const double cycles = time * m_front.cascade[static_cast<size_t>(cascadeIndex)].loopDivisor / m_front.key.period;
    double phase = cycles - std::floor(cycles);
    double position = phase * m_front.frames;
    if (position >= m_front.frames) position -= m_front.frames;
    return std::max(position, 0.0);
}

bool OceanSimulation::ensurePrograms(int resolution)
{
    if (m_programResolution == resolution && m_spectrumProgram != 0) return true;
    releasePrograms();
    m_spectrumProgram = buildCompute("ocean_spectrum.comp", resolution);
    m_fftProgram = buildCompute("ocean_fft.comp", resolution);
    m_finalizeProgram = buildCompute("ocean_finalize.comp", resolution);
    m_foamProgram = buildCompute("ocean_foam.comp", resolution);
    m_mipProgram = buildCompute("ocean_mip.comp", resolution);
    if (m_spectrumProgram == 0 || m_fftProgram == 0 || m_finalizeProgram == 0 || m_foamProgram == 0 || m_mipProgram == 0) {
        releasePrograms();
        return false;
    }
    m_programResolution = resolution;
    return true;
}

bool OceanSimulation::startJob(const Key& key)
{
    const QualityInfo q = qualityInfo(key.quality);
    if (!ensurePrograms(q.resolution)) return false;

    Set set;
    set.key = key;
    set.cascades = q.cascades;
    set.resolution = q.resolution;
    set.frames = q.frames;
    set.levels = 1;
    while ((1 << (set.levels - 1)) < q.resolution) ++set.levels;

    // Cascade bands: from an eighth of the peak wave number to 4 cm waves, B per cascade.
    const Jonswap jonswap = jonswapFor(key);
    const double peakK = jonswap.peakOmega * jonswap.peakOmega / kGravity;
    const double kMin = peakK / 8.0;
    const double kMax = 2.0 * kPi / kShortestWave;
    const double framesRatio = static_cast<double>(q.frames) / (kQuantisation * kFramesPerPeriod);
    double band = std::min({ q.resolution / 8.0, framesRatio * framesRatio, std::pow(std::max(kMax / kMin, 2.0), 1.0 / q.cascades) });
    band = std::max(band, 2.0);
    const double period = std::max(static_cast<double>(key.period), 1.0);
    const int n = q.resolution;
    // Waves travel downwind: towards windDirection + 180 degrees (azimuth around +Y from +Z towards +X).
    const double travel = (static_cast<double>(key.windDirection) + 180.0) * kPi / 180.0;
    const double travelX = std::sin(travel), travelZ = std::cos(travel);

    std::vector<float> spectrum(static_cast<size_t>(n) * n * 4u * q.cascades, 0.0f);
    std::vector<double> amplitude(static_cast<size_t>(n) * n * 2u);
    double resolvedSlope = 0.0, heightVariance = 0.0;
    for (int c = 0; c < q.cascades; ++c) {
        Cascade& cascade = set.cascade[static_cast<size_t>(c)];
        cascade.kLow = static_cast<float>(kMin * std::pow(band, c));
        cascade.kHigh = static_cast<float>(cascade.kLow * band);
        // The band's top lies at a quarter of the texel rate (4 texels per shortest wave).
        cascade.patchSize = static_cast<float>(2.0 * kPi * (n / 4.0) / cascade.kHigh);
        const double lowOmega = std::sqrt(kGravity * cascade.kLow);
        cascade.loopDivisor = std::max(1, static_cast<int>(std::floor(lowOmega * period / (2.0 * kPi * kQuantisation))));
        const double dk = 2.0 * kPi / cascade.patchSize;
        const double texel = cascade.patchSize / n;
        cascade.unresolvedSlopeVariance.fill(0.0f);
        double slope = 0.0, height = 0.0;
        std::array<double, kSlopeLods> unresolved{};
        for (int z = 0; z < n; ++z) {
            const int nz = z < n / 2 ? z : z - n;
            for (int x = 0; x < n; ++x) {
                const int nx = x < n / 2 ? x : x - n;
                const double kx = dk * nx, kz = dk * nz;
                const double k = std::sqrt(kx * kx + kz * kz);
                double a = 0.0;
                if (k >= cascade.kLow && k < cascade.kHigh && k > 0.0) {
                    const double theta = std::atan2(kx * travelZ - kz * travelX, kx * travelX + kz * travelZ);
                    const double s = jonswap.waveNumberSpectrum(k, theta);
                    const double variance = s * dk * dk; // E |h~(k)|^2 summed over the grid = height variance
                    height += variance;
                    slope += k * k * variance;
                    for (int lod = 1; lod < kSlopeLods; ++lod) {
                        if (k > kPi / (2.0 * texel * std::ldexp(1.0, lod))) unresolved[static_cast<size_t>(lod)] += k * k * variance;
                    }
                    a = std::sqrt(variance / 2.0);
                }
                // h0 = (xi_r + i xi_i) / sqrt(2) x sqrt(S dk^2 / 2), Box-Muller from the texel hash.
                const double u1 = uniform01(static_cast<uint32_t>(c), static_cast<uint32_t>(x), static_cast<uint32_t>(z), 0x51ED270BU);
                const double u2 = uniform01(static_cast<uint32_t>(c), static_cast<uint32_t>(x), static_cast<uint32_t>(z), 0xA2C2A3B1U);
                const double radius = std::sqrt(-2.0 * std::log(u1));
                const size_t i = (static_cast<size_t>(z) * n + x) * 2u;
                amplitude[i] = radius * std::cos(2.0 * kPi * u2) * a / std::sqrt(2.0);
                amplitude[i + 1] = radius * std::sin(2.0 * kPi * u2) * a / std::sqrt(2.0);
            }
        }
        // RGBA: h0(k), conj(h0(-k)).
        float* layer = spectrum.data() + static_cast<size_t>(n) * n * 4u * c;
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const size_t i = (static_cast<size_t>(z) * n + x) * 2u;
                const size_t m = (static_cast<size_t>((n - z) % n) * n + static_cast<size_t>((n - x) % n)) * 2u;
                float* texel4 = layer + (static_cast<size_t>(z) * n + x) * 4u;
                texel4[0] = static_cast<float>(amplitude[i]);
                texel4[1] = static_cast<float>(amplitude[i + 1]);
                texel4[2] = static_cast<float>(amplitude[m]);
                texel4[3] = static_cast<float>(-amplitude[m + 1]);
            }
        }
        cascade.slopeVariance = static_cast<float>(slope);
        cascade.heightVariance = static_cast<float>(height);
        for (int lod = 0; lod < kSlopeLods; ++lod) {
            cascade.unresolvedSlopeVariance[static_cast<size_t>(lod)] = static_cast<float>(unresolved[static_cast<size_t>(lod)]);
        }
        resolvedSlope += slope;
        heightVariance += height;
    }
    // Cox and Munk (1954), clean surface: total mean square slope 0.003 + 5.12e-3 U. What the cascades do not resolve
    // becomes roughness.
    const double coxMunk = 0.003 + 5.12e-3 * jonswap.windSpeed;
    set.tailSlopeVariance = static_cast<float>(std::max(coxMunk - resolvedSlope, 0.0));
    set.significantWaveHeight = static_cast<float>(4.0 * std::sqrt(heightVariance));

    // GL objects: the spectrum and scratch textures (kept between jobs), the new set's array.
    if (m_scratchResolution != n) {
        if (m_spectrumTexture != 0) glDeleteTextures(1, &m_spectrumTexture);
        if (m_scratchTexture != 0) glDeleteTextures(1, &m_scratchTexture);
        m_spectrumTexture = m_scratchTexture = 0;
        glGenTextures(1, &m_spectrumTexture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_spectrumTexture);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA32F, n, n, kMaxCascades);
        glGenTextures(1, &m_scratchTexture);
        glBindTexture(GL_TEXTURE_2D, m_scratchTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, n, n);
        m_scratchResolution = n;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_spectrumTexture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, n, n, q.cascades, GL_RGBA, GL_FLOAT, spectrum.data());

    glGenTextures(1, &set.texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, set.texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, set.levels, GL_RGBA16F, n, n, q.cascades * q.frames);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, set.levels - 1);
    uint64_t texels = 0;
    for (int level = 0; level < set.levels; ++level) {
        const uint64_t side = static_cast<uint64_t>(std::max(n >> level, 1));
        texels += side * side;
    }
    set.bytes = texels * 8u * static_cast<uint64_t>(q.cascades * q.frames);

    m_job = Job();
    m_job.active = true;
    m_job.set = set;
    m_job.itemCount = q.cascades * q.frames + 3 * q.cascades;
    return true;
}

// One piece of work: a frame of a cascade (spectrum at that time, row and column inverse FFT, displacement and
// Jacobian), the foam accumulation of a cascade (two turns of its loop), or the mip chain.
void OceanSimulation::runItem(int item)
{
    Set& set = m_job.set;
    const int n = set.resolution;
    const int groups = n / 8;
    const int frameItems = set.cascades * set.frames;
    if (item < frameItems) {
        const int c = item / set.frames;
        const int frame = item % set.frames;
        const Cascade& cascade = set.cascade[static_cast<size_t>(c)];
        const double omega0 = 2.0 * kPi * cascade.loopDivisor / std::max(static_cast<double>(set.key.period), 1.0);

        glUseProgram(m_spectrumProgram);
        glBindImageTexture(0, m_scratchTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, m_spectrumTexture, 0, GL_FALSE, c, GL_READ_ONLY, GL_RGBA32F);
        glUniform1f(glGetUniformLocation(m_spectrumProgram, "u_PatchSize"), cascade.patchSize);
        glUniform1f(glGetUniformLocation(m_spectrumProgram, "u_Omega0"), static_cast<float>(omega0));
        glUniform1i(glGetUniformLocation(m_spectrumProgram, "u_Frame"), frame);
        glUniform1i(glGetUniformLocation(m_spectrumProgram, "u_Frames"), set.frames);
        glUniform1f(glGetUniformLocation(m_spectrumProgram, "u_Choppiness"), std::max(set.key.choppiness, 0.0f));
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(m_fftProgram);
        glBindImageTexture(0, m_scratchTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        const GLint horizontal = glGetUniformLocation(m_fftProgram, "u_Horizontal");
        glUniform1i(horizontal, 1);
        glDispatchCompute(n, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glUniform1i(horizontal, 0);
        glDispatchCompute(n, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(m_finalizeProgram);
        glBindImageTexture(0, m_scratchTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(2, set.texture, 0, GL_FALSE, c * set.frames + frame, GL_WRITE_ONLY, GL_RGBA16F);
        glUniform1f(glGetUniformLocation(m_finalizeProgram, "u_Texel"), cascade.patchSize / n);
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    } else {
        // Per cascade, after all its frames: the two foam turns, then its mip chain.
        const int c = (item - frameItems) / 3;
        const int step = (item - frameItems) % 3;
        const Cascade& cascade = set.cascade[static_cast<size_t>(c)];
        if (step < 2) {
            const double frameSeconds =
                std::max(static_cast<double>(set.key.period), 1.0) / (cascade.loopDivisor * set.frames);
            glUseProgram(m_foamProgram);
            glBindImageTexture(2, set.texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
            glUniform1f(glGetUniformLocation(m_foamProgram, "u_Recovery"), static_cast<float>(kFoamRecoveryPerSecond * frameSeconds));
            const GLint layer = glGetUniformLocation(m_foamProgram, "u_Layer");
            const GLint previous = glGetUniformLocation(m_foamProgram, "u_PreviousLayer");
            // Two turns: the second starts from the end of the first, so frame 0 also carries the foam of the loop's end.
            for (int frame = 0; frame < set.frames; ++frame) {
                glUniform1i(layer, c * set.frames + frame);
                glUniform1i(previous, c * set.frames + (frame + set.frames - 1) % set.frames);
                glDispatchCompute(groups, groups, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
        } else {
            // The cascade's mip chain: 2 x 2 box averages, level by level, all its frames at once.
            glUseProgram(m_mipProgram);
            const GLint base = glGetUniformLocation(m_mipProgram, "u_LayerBase");
            const GLint size = glGetUniformLocation(m_mipProgram, "u_Size");
            glUniform1i(base, c * set.frames);
            for (int level = 1; level < set.levels; ++level) {
                const int side = std::max(n >> level, 1);
                glBindImageTexture(0, set.texture, level - 1, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
                glBindImageTexture(2, set.texture, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glUniform1i(size, side);
                glDispatchCompute((side + 7) / 8, (side + 7) / 8, set.frames);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }
}

void OceanSimulation::finishJob()
{
    destroy(m_front);
    m_front = m_job.set;
    m_front.version = m_nextVersion++;
    m_stats.lastPrecomputeMs = m_job.wallMs;
    m_stats.lastPrecomputeGpuMs = m_job.gpuMs;
    m_stats.maxSliceGpuMs = m_job.maxSliceMs;
    m_stats.slicesGpuMs = m_job.slices;
    m_stats.lastPrecomputeFrames = m_job.appFrames;
    ++m_stats.sets;
    m_finishedSerial = m_job.serial; // slices still in flight are added to the stats when they are read
    m_job = Job();
}

void OceanSimulation::abortJob()
{
    if (!m_job.active) return;
    destroy(m_job.set);
    m_job = Job();
    ++m_stats.aborted;
}

double OceanSimulation::itemUnits(int item) const
{
    // Relative GPU cost: a frame (spectrum, 2 FFT passes, finalize) is 1; a foam turn of a cascade (F light passes)
    // and a cascade's mip chain each about F / 8.
    const Set& set = m_job.set;
    const int frameItems = set.cascades * set.frames;
    if (item < frameItems) return 1.0;
    // Measured (RTX class GPU, medium): a foam turn and a mip chain each cost about F / 8 frames.
    return std::max(1.0, set.frames / 8.0);
}

void OceanSimulation::pollTimers(bool wait)
{
    for (SliceTimer& timer : m_timers) {
        if (!timer.pending) continue;
        GLint available = 0;
        if (!wait) glGetQueryObjectiv(timer.queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!wait && !available) continue;
        GLuint64 t0 = 0, t1 = 0;
        glGetQueryObjectui64v(timer.queries[0], GL_QUERY_RESULT, &t0);
        glGetQueryObjectui64v(timer.queries[1], GL_QUERY_RESULT, &t1);
        const double gpuMs = t1 > t0 ? static_cast<double>(t1 - t0) / 1.0e6 : 0.0;
        timer.pending = false;
        m_stats.lastSliceGpuMs = gpuMs;
        // The estimate follows the dearest recent measurement (a small first slice under-reads the cost per unit).
        if (timer.units > 0.0) m_costPerUnitMs = std::max(gpuMs / timer.units, 0.75 * m_costPerUnitMs);
        if (m_job.active && timer.job == m_job.serial) {
            m_job.gpuMs += gpuMs;
            m_job.maxSliceMs = std::max(m_job.maxSliceMs, gpuMs);
            if (m_job.slices.size() < 256) m_job.slices.push_back(static_cast<float>(gpuMs));
        } else if (timer.job == m_finishedSerial) {
            m_stats.lastPrecomputeGpuMs += gpuMs;
            m_stats.maxSliceGpuMs = std::max(m_stats.maxSliceGpuMs, gpuMs);
            if (m_stats.slicesGpuMs.size() < 256) m_stats.slicesGpuMs.push_back(static_cast<float>(gpuMs));
        }
    }
}

bool OceanSimulation::update(const OceanSettings& settings, bool synchronous, double budgetMs)
{
    if (!oceanAvailable()) return false;
    pollTimers(false);
    const Key key = keyOf(settings);
    const bool frontCurrent = m_front.texture != 0 && m_front.key == key;
    if (frontCurrent && !m_job.active) {
        m_stats.lastSliceItems = 0;
        return true;
    }
    if (m_failed && !(m_failedKey == key)) {
        m_failed = false; // a new setting may succeed again
    }
    if (m_failed) return false;

    SavedState saved;
    saved.save();
    const auto wallStart = profiling::Clock::now();
    if (frontCurrent && m_job.active) {
        abortJob(); // the settings went back to the front set
    } else if (m_job.active && !(m_job.set.key == key)) {
        abortJob();
    }
    if (!frontCurrent && !m_job.active) {
        if (!startJob(key)) {
            m_failed = true;
            m_failedKey = key;
            saved.restore();
            return false;
        }
        m_job.serial = ++m_jobSerial;
        // The first set (nothing to show yet) is built at once.
        if (m_front.texture == 0) synchronous = true;
    }
    if (!m_job.active) {
        saved.restore();
        return true;
    }
    if (m_timers[0].queries[0] == 0) {
        for (SliceTimer& timer : m_timers) glGenQueries(2, timer.queries);
    }

    ++m_job.appFrames;
    // Every slice is timed (GL_TIMESTAMP before and after) when a timer is free; the measured cost per unit of the
    // latest slice sets how much of the job fits into the next frame's budget.
    SliceTimer* timer = nullptr;
    for (SliceTimer& candidate : m_timers) {
        if (!candidate.pending) {
            timer = &candidate;
            break;
        }
    }
    const int start = m_job.nextItem;
    double units = 0.0;
    if (timer != nullptr) glQueryCounter(timer->queries[0], GL_TIMESTAMP);
    if (synchronous) {
        while (m_job.nextItem < m_job.itemCount) {
            units += itemUnits(m_job.nextItem);
            runItem(m_job.nextItem++);
        }
    } else {
        const double budget = std::max(budgetMs, 0.05);
        // At most twice the previous slice's work (a ramp: 1, 2, 4, ... units), so one under-read measurement cannot
        // put the whole set into one frame.
        const double limit = std::max(2.0 * m_job.lastSliceUnits, 1.0);
        do {
            const double next = itemUnits(m_job.nextItem);
            // Without a measurement yet: one item per frame.
            if (m_job.nextItem > start &&
                (m_costPerUnitMs <= 0.0 || (units + next) * m_costPerUnitMs > budget || units + next > limit)) break;
            units += next;
            runItem(m_job.nextItem++);
        } while (m_job.nextItem < m_job.itemCount);
    }
    if (timer != nullptr) {
        glQueryCounter(timer->queries[1], GL_TIMESTAMP);
        timer->pending = true;
        timer->units = units;
        timer->job = m_job.serial;
    } else {
        m_job.gpuMs += units * m_costPerUnitMs; // estimated: every timer is still in flight
    }
    m_stats.lastSliceItems = m_job.nextItem - start;
    m_job.lastSliceUnits = units;
    if (synchronous) {
        glFinish(); // the wall time of a synchronous set includes its GPU work
        pollTimers(true);
    }
    m_job.wallMs += profiling::elapsedMs(wallStart);
    if (m_job.nextItem >= m_job.itemCount) {
        finishJob();
    }
    saved.restore();
    return true;
}

} // namespace ux3d::imgui
