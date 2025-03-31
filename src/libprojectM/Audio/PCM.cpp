#include "PCM.hpp"
#include <Windows.h>
#include <algorithm>

namespace libprojectM {
namespace Audio {

template <typename T>
T clamp(T value, T minVal, T maxVal) {
    return std::max(minVal, std::min(value, maxVal));
}

constexpr auto PI = 3.14159265f;

template<
    int signalAmplitude,
    int signalOffset,
    typename SampleType>
void PCM::AddToBuffer(
    SampleType const* const samples,
    uint32_t channels,
    size_t const sampleCount)
{
    if (channels == 0 || sampleCount == 0)
    {
        return;
    }

    for (size_t i = 0; i < sampleCount; i++)
    {
        size_t const bufferOffset = (m_start + i) % AudioBufferSamples;
        m_inputBufferL[bufferOffset] = 128.0f * (static_cast<float>(samples[0 + i * channels]) - float(signalOffset)) / float(signalAmplitude);
        if (channels > 1)
        {
            m_inputBufferR[bufferOffset] = 128.0f * (static_cast<float>(samples[1 + i * channels]) - float(signalOffset)) / float(signalAmplitude);
        }
        else
        {
            m_inputBufferR[bufferOffset] = m_inputBufferL[bufferOffset];
        }
    }
    m_start = (m_start + sampleCount) % AudioBufferSamples;
}

void PCM::Add(float const* const samples, uint32_t channels, size_t const count)
{
    AddToBuffer<1, 0>(samples, channels, count);
}
void PCM::Add(uint8_t const* const samples, uint32_t channels, size_t const count)
{
    AddToBuffer<128, 128>(samples, channels, count);
}
void PCM::Add(int16_t const* const samples, uint32_t channels, size_t const count)
{
    AddToBuffer<32768, 0>(samples, channels, count);
}

// Used for figuring out if RMS has any effect
void DebugPrint(const char* message) {
    OutputDebugStringA(message);
}

void PCM::ScaleAndBassBoost(float volume, float bass)
{
    float scaleFactor = 1.0f + volume * 0.5f;  // Scale by loudness
    float bassBoostFactor = 1.0f + bass * 0.7f;  // Boost low frequencies

    for (size_t i = 0; i < WaveformSamples; i++)
    {
        float bassWeight = 1.0f - (static_cast<float>(i) / WaveformSamples); // Stronger bass impact at low frequencies
        float boost = scaleFactor * (1.0f + bassBoostFactor * bassWeight);

        m_waveformL[i] *= boost;
        m_waveformR[i] *= boost;
    }
}

void PCM::SmoothSpectrum()
{
    for (size_t i = 1; i < SpectrumSamples - 1; i++)
    {
        m_spectrumL[i] = ((m_spectrumL[i - 1] + m_spectrumL[i] + m_spectrumL[i + 1]) / 3.0f);
        m_spectrumR[i] = ((m_spectrumR[i - 1] + m_spectrumR[i] + m_spectrumR[i + 1]) / 3.0f);
    }
}

void PCM::SmoothHarmonicSpectrum()
{
    for (size_t i = 1; i < SpectrumSamples - 1; i++)
    {
        m_harmonicSpectrum[i] = ((m_harmonicSpectrum[i - 1] + m_harmonicSpectrum[i] + m_harmonicSpectrum[i + 1]) / 3.0f);
    }
}

void PCM::ComputeSF()
{
    float flux = 0.0f;
    float diffL = 0.0f;
    for (size_t i = 0; i < SpectrumSamples / 2 - 1; i++)
    {
        diffL = abs(m_spectrumL[i]) - abs(m_prevSpectrumL[i]);
        flux += diffL * diffL;
    }
    m_spectralFlux = sqrt(flux) * 10.0 / (SpectrumSamples / 2);
    m_prevSpectrumL = m_spectrumL;
}

void PCM::ApplyLPF()
{
    static float prevL = 0.0f;
    static float prevR = 0.0f;

    float cutoffFreq = std::clamp(m_spectralPredictivity * 300, 50.0f, 1000.0f);
    float RC = 1.0f / (2.0f * PI * cutoffFreq);
    float alpha = 0.00002267573 / (RC + 0.00002267573);

    for (size_t i = 0; i < WaveformSamples; i++)
    {
        m_waveformL[i] = alpha * m_waveformL[i] + (1.0f - alpha) * prevL;
        m_waveformR[i] = alpha * m_waveformR[i] + (1.0f - alpha) * prevR;

        prevL = m_waveformL[i];
        prevR = m_waveformR[i];
    }
}

void PCM::ApplyHPS()
{
    constexpr int Lp = 50;  // Median filter window size
    constexpr int halfLp = Lp / 2;

    constexpr int minFreqIdx = 200;
    constexpr int maxFreqIdx = 450;

    for (size_t i = halfLp; i < SpectrumSamples - halfLp; i++)
    {
        // Makeshift filter, dampends values that are outside of filter range
        if (i < minFreqIdx || i > maxFreqIdx)
        {
            m_harmonicSpectrum[i] = m_spectrumL[i] * 0.1f;
            continue;
        }

        std::array<float, Lp> neighborhood{};

        for (int j = 0; j < Lp; j++)
        {
            int idx = std::clamp(static_cast<int>(i + j - halfLp), 0, static_cast<int>(SpectrumSamples - 1));
            neighborhood[j] = m_spectrumL[idx];
        }

        float avgrSpecrtum = (m_spectrumL[i] + m_prevSpectrumL[i] + m_prevPrevSpectrumL[i]) / 3.0f;

        std::nth_element(neighborhood.begin(), neighborhood.begin() + halfLp, neighborhood.end());

        if (m_spectrumL[i] >= neighborhood[halfLp])
        {
            m_harmonicSpectrum[i] = (m_spectrumL[i] + m_prevSpectrumL[i] + m_prevPrevSpectrumL[i]) / 3 <= 0.5 ? pow(m_spectrumL[i], 2.0f) : m_spectrumL[i] /* * exp( - m_spectralFlux * 30.0f)*/;
        }
        else
        {
            m_harmonicSpectrum[i] = 0.0f;
        }
    }
    m_prevPrevSpectrumL = m_prevSpectrumL;
    m_prevSpectrumL = m_spectrumL;
}

void PCM::ApplyTransientDetection()
{    
    for (size_t i = 0; i < SpectrumSamples; i++)
    {
        // Rolling average to reduce sensitivity to transients
        float smoothedValue = (m_spectrumL[i] + m_prevSpectrumL[i] + m_prevPrevSpectrumL[i]) / 3.0f;

        float transient = m_spectrumL[i] - (m_prevSpectrumL[i] + m_prevPrevSpectrumL[i]);

        // Define transient threshold (adjustable)
        constexpr float transientThreshold = 0.75f;

        // Check if it's a transient
        if (transient < 0)
        {
            m_harmonicSpectrum[i] = m_spectrumL[i];  // Keep stable frequencies
        }
        else
        {
            m_harmonicSpectrum[i] = m_spectrumL[i] * 0.1f;  // Limit transient peaks
        }
    }
    m_prevPrevSpectrumL = m_prevSpectrumL;
    m_prevSpectrumL = m_spectrumL;
}


void PCM::ComputeSP()
{
    float predictivity = 0.0f;

    // Use only the **top end** (high frequencies) for predictivity calculation
    size_t startIdx = SpectrumSamples * 2 / 3; // Start at 66% of the spectrum (~8kHz+)

    for (size_t i = startIdx; i < SpectrumSamples - 1; i++)
    {
        // Predict the next frame using the last two frames
        float predictedL = 2 * m_prevSpectrumL[i] - m_prevPrevSpectrumL[i];

        // Compute squared error
        float errorL = (m_spectrumL[i] - predictedL) * (m_spectrumL[i] - predictedL);

        predictivity += errorL;
    }

    // Normalize & boost the value to make it more noticeable
    m_spectralPredictivity = sqrt(predictivity) * 1000.0f / (SpectrumSamples / 4);

    // Shift stored spectra for next frame
    m_prevPrevSpectrumL = m_prevSpectrumL;
    m_prevSpectrumL = m_spectrumL;
}

void PCM::UpdateFrameAudioData(double secondsSinceLastFrame, uint32_t frame)
{
    // 1. Copy audio data from input buffer
    CopyNewWaveformData(m_inputBufferL, m_waveformL);
    CopyNewWaveformData(m_inputBufferR, m_waveformR);


    // 2. Update spectrum analyzer data for both channels
    UpdateSpectrum(m_waveformL, m_spectrumL);
    UpdateSpectrum(m_waveformR, m_spectrumR);

    // 4. Update beat detection values
    m_bass.Update(m_spectrumL, secondsSinceLastFrame, frame);
    m_middles.Update(m_spectrumL, secondsSinceLastFrame, frame);
    m_treble.Update(m_spectrumL, secondsSinceLastFrame, frame);
    m_volume.UpdateVolume(m_waveformL, secondsSinceLastFrame, frame);

    float bass = m_bass.CurrentRelative();
    float volume = m_volume.CurrentRelative();

    ScaleAndBassBoost(volume, bass);
    // ApplyLPF();
    ComputeSF();
    ComputeSP();
    // ApplyTransientDetection();
    ApplyHPS();
    // SmoothHarmonicSpectrum();

    // 3. Align waveforms
    m_alignL.Align(m_waveformL);
    m_alignR.Align(m_waveformR);

}

auto PCM::GetFrameAudioData() const -> FrameAudioData
{
    FrameAudioData data{};

    std::copy(m_waveformL.begin(), m_waveformL.begin() + WaveformSamples, data.waveformLeft.begin());
    std::copy(m_waveformR.begin(), m_waveformR.begin() + WaveformSamples, data.waveformRight.begin());
    std::copy(m_harmonicSpectrum.begin(), m_harmonicSpectrum.begin() + SpectrumSamples, data.spectrumLeft.begin());
    std::copy(m_harmonicSpectrum.begin(), m_harmonicSpectrum.begin() + SpectrumSamples, data.spectrumRight.begin());
    // std::copy(m_spectrumL.begin(), m_spectrumL.begin() + SpectrumSamples, data.spectrumLeft.begin());
    // std::copy(m_spectrumR.begin(), m_spectrumR.begin() + SpectrumSamples, data.spectrumRight.begin());

    data.bass = m_bass.CurrentRelative();
    data.mid = m_middles.CurrentRelative();
    data.treb = m_treble.CurrentRelative();

    data.bassAtt = m_bass.AverageRelative();
    data.midAtt = m_middles.AverageRelative();
    data.trebAtt = m_treble.AverageRelative();

    data.vol = m_volume.CurrentRelative();
    data.volAtt = m_volume.AverageRelative();
    data.spectralFlux = m_spectralFlux;
    
    // char debugMsg[512];
    // sprintf(debugMsg, "data.vol = %f    data.bass = %f   data.mid = %f   data.treb = %f\n", data.vol, data.bass, data.mid, data.treb);
    // sprintf(debugMsg, "data.SF = %f    data.SF2 = %f\n", exp( - m_spectralFlux * 30.0f), m_spectralFlux);
    // OutputDebugStringA(debugMsg);

    // data.vol = (data.bass + data.mid + data.treb) * 0.333f;
    // data.volAtt = (data.bassAtt + data.midAtt + data.trebAtt) * 0.333f;

    return data;
}

void PCM::UpdateSpectrum(const WaveformBuffer& waveformData, SpectrumBuffer& spectrumData)
{
    std::vector<float> waveformSamples(AudioBufferSamples);
    std::vector<float> spectrumValues;

    size_t oldI{0};
    for (size_t i = 0; i < AudioBufferSamples; i++)
    {
        // Damp the input into the FFT a bit, to reduce high-frequency noise:
        waveformSamples[i] = 0.5f * (waveformData[i] + waveformData[oldI]);
        oldI = i;
    }

    m_fft.TimeToFrequencyDomain(waveformSamples, spectrumValues);

    std::copy(spectrumValues.begin(), spectrumValues.end(), spectrumData.begin());
}

void PCM::CopyNewWaveformData(const WaveformBuffer& source, WaveformBuffer& destination)
{
    auto const bufferStartIndex = m_start.load();

    for (size_t i = 0; i < AudioBufferSamples; i++)
    {
        destination[i] = source[(bufferStartIndex + i) % AudioBufferSamples];
    }
}

} // namespace Audio
} // namespace libprojectM
