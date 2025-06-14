#include "Loudness.hpp"
#include <algorithm>
#include <cmath>
// #include <Windows.h>

namespace libprojectM {
namespace Audio {

template <typename T>
T clamp(T value, T minVal, T maxVal) {
    return std::max(minVal, std::min(value, maxVal));
}

Loudness::Loudness(Loudness::Band band)
    : m_band(band)
{
}

// void DebugPrint(const char* message) {
//     OutputDebugStringA(message);
// }

void Loudness::ComputeRMS(const WaveformBuffer& m_waveformL)
{
    float prevFilteredSample = 0.0f;
    float alpha = 0.001;
    m_RMS = 0.0f;
    for (size_t i = 0; i < AudioBufferSamples; i++)
    {
        float filteredSample = alpha * m_waveformL[i] + (1 - alpha) * prevFilteredSample;   //applies a low pass filter to the sample.
        prevFilteredSample = filteredSample;
        m_RMS += filteredSample * filteredSample;
    }
    m_RMS = sqrt(m_RMS / AudioBufferSamples);

    // char debugMsg[512];
    // sprintf(debugMsg, "DEBUG: m_RMS = %f\n", m_RMS);
    // OutputDebugStringA(debugMsg);
}

void Loudness::UpdateVolume(const WaveformBuffer& m_waveformL, double secondsSinceLastFrame, uint32_t frame)
{
    ComputeRMS(m_waveformL);
    m_current = m_RMS;
    UpdateBandAverage(secondsSinceLastFrame, frame);
}

void Loudness::Update(const std::array<float, SpectrumSamples>& spectrumSamples, double secondsSinceLastFrame, uint32_t frame)
{
    SumBand(spectrumSamples);
    UpdateBandAverage(secondsSinceLastFrame, frame);
}

auto Loudness::CurrentRelative() const -> float
{
    return m_currentRelative;
}

auto Loudness::AverageRelative() const -> float
{
    return m_averageRelative;
}

void Loudness::SumBand(const std::array<float, SpectrumSamples>& spectrumSamples)
{
    int start = SpectrumSamples * static_cast<int>(m_band) / 6;
    int end = SpectrumSamples * (static_cast<int>(m_band) + 1) / 6;

    m_current = 0.0f;
    for (int sample = start; sample < end; sample++)
    {
        m_current += spectrumSamples[sample];
    }
}

void Loudness::UpdateBandAverage(double secondsSinceLastFrame, uint32_t frame)
{
    float rate = AdjustRateToFps(m_current > m_average ? 0.2f : 0.5f, secondsSinceLastFrame);
    m_average = m_average * rate + m_current * (1.0f - rate);

    rate = AdjustRateToFps(frame < 50 ? 0.9f : 0.992f, secondsSinceLastFrame);
    m_longAverage = m_longAverage * rate + m_current * (1.0f - rate);

    m_currentRelative = std::fabs(m_longAverage) < 0.001f ? 1.0f : m_current / m_longAverage;
    m_averageRelative = std::fabs(m_longAverage) < 0.001f ? 1.0f : m_average / m_longAverage;
}

auto Loudness::AdjustRateToFps(float rate, double secondsSinceLastFrame) -> float
{
    float const perSecondDecayRateAtFps1 = std::pow(rate, 30.0f);
    float const perFrameDecayRateAtFps2 = std::pow(perSecondDecayRateAtFps1, static_cast<float>(secondsSinceLastFrame));

    return perFrameDecayRateAtFps2;
}

} // namespace Audio
} // namespace libprojectM
