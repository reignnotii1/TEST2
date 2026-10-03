#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

const StringArray ChopProcessor::chopNames { "1/1","1/2","1/4","1/8","1/8T","1/16","1/16T","1/32","1/32T","1/64" };
const double ChopProcessor::chopBeats[10]  { 4.0, 2.0, 1.0, 0.5, 1.0/3.0, 0.25, 1.0/6.0, 0.125, 1.0/12.0, 0.0625 };
const StringArray ChopProcessor::refreshNames { "1/32","1/16","1/8","1/4","1/2","1/1","2 bar","4 bar" };
const double ChopProcessor::refreshBeats[8] { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };

static String pct (float v, int) { return String (roundToInt (v)) + "%"; }
static String db  (float v, int) { return String (v >= 0 ? "+" : "") + String (v, 1) + " dB"; }

AudioProcessorValueTreeState::ParameterLayout ChopProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chop", 1 },    "Chop",    chopNames,    7)); // 1/32
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "refresh", 1 }, "Refresh", refreshNames, 3)); // 1/4
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "freeze", 1 },  "Freeze",  false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 },  "Gate",   NormalisableRange<float> (1.f, 100.f, 0.1f), 20.f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fade", 1 },  "Fade",   NormalisableRange<float> (0.f, 100.f, 0.1f), 25.f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 },   "Mix",    NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    return l;
}

ChopProcessor::ChopProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool ChopProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void ChopProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    for (auto& h : hold) h.assign ((size_t) (sr * 16.0), 0.0f);   // enough for a 1/1 chop down to 15 BPM
    holdValid = false;
    mixSm.reset (sr, 0.02);  mixSm.setCurrentAndSetTargetValue (apvts.getRawParameterValue ("mix")->load() / 100.f);
    outSm.reset (sr, 0.02);  outSm.setCurrentAndSetTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
}

void ChopProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n   = buf.getNumSamples();
    const int nch = jmin (buf.getNumChannels(), 2);

    // ---- transport -------------------------------------------------------
    double bpm = 120.0, ppq = freeRunPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = jmax (20.0, *b);
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    const double bps = bpm / 60.0 / sampleRate;   // beats per sample

    // ---- parameters --------------------------------------------------------
    const double cb = chopBeats    [jlimit (0, 9, (int) apvts.getRawParameterValue ("chop")->load())];
    const double rb = refreshBeats [jlimit (0, 7, (int) apvts.getRawParameterValue ("refresh")->load())];
    const bool frozen = apvts.getRawParameterValue ("freeze")->load() > 0.5f;
    const double gate = jmax (0.01, apvts.getRawParameterValue ("gate")->load() / 100.0);
    const double fade = apvts.getRawParameterValue ("fade")->load() / 100.0;
    mixSm.setTargetValue (apvts.getRawParameterValue ("mix")->load() / 100.f);
    outSm.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    const double chopLen = cb / bps;                         // samples per chop
    const int holdSize = (int) hold[0].size();
    float lastPhase = 0.f;

    for (int i = 0; i < n; ++i)
    {
        const double t = ppq + i * bps;
        const double chopStart = std::floor (t / cb) * cb;
        const double pos = ((t - chopStart) / cb) * chopLen; // samples into this chop
        const double refStart = std::floor (t / rb) * rb;

        // First chop of every refresh window plays live and is recorded; the rest of the
        // window replays that captured slice (the "chop/stutter"). Freeze stops re-capturing.
        const bool first = chopStart < refStart + cb * 0.999;
        const bool live  = (! frozen && first) || ! holdValid;
        const int idx = jlimit (0, holdSize - 1, (int) pos);

        // gate + fade envelope
        const double gateLen = gate * chopLen;
        double env = 0.0;
        if (pos < gateLen)
        {
            const double fs = jmin (jmax (32.0, fade * 0.5 * gateLen), gateLen * 0.5);
            double e = jmin (1.0, jmin (pos / fs, (gateLen - pos) / fs));
            env = e * e * (3.0 - 2.0 * e);                   // smoothstep
        }

        const float m = mixSm.getNextValue(), g = outSm.getNextValue();
        for (int ch = 0; ch < nch; ++ch)
        {
            float* d = buf.getWritePointer (ch);
            const float x = d[i];
            float y;
            if (live) { hold[ch][(size_t) idx] = x; y = x; }
            else        y = hold[ch][(size_t) idx];
            d[i] = (x * (1.f - m) + y * (float) env * m) * g;
        }
        if (live) holdValid = true;
        lastPhase = (float) jlimit (0.0, 1.0, (t - refStart) / rb);
    }

    freeRunPpq = ppq + n * bps;
    uiRefreshPhase.store (lastPhase);
}

AudioProcessorEditor* ChopProcessor::createEditor() { return new ChopEditor (*this); }

void ChopProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void ChopProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChopProcessor(); }
