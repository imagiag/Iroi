// Host smoke test of the whole Resonator: original mode, V/Oct mode (CV steps,
// feedback up to infinite, dissonance), switching back and forth. Fails on
// NaN/inf or runaway output, and prints the relative cost of the two modes.
#include "Resonator.h"
#include <chrono>
#include <cstdio>

extern "C" void assert_failed(const char* msg, const char* location, int line)
{
    fprintf(stderr, "assert failed: %s %s:%d\n", msg, location, line);
    exit(2);
}
extern "C" float randf() { return rand() / (float)RAND_MAX; }
extern "C" float fast_powf(float x, float y) { return (powf)(x, y); } // OWL has a table version on the module
AudioBuffer::~AudioBuffer() {}

class Buffer : public AudioBuffer {
public:
    FloatArray ch[2];
    int size;
    Buffer(int n) : size(n) { ch[0] = FloatArray::create(n); ch[1] = FloatArray::create(n); }
    FloatArray getSamples(int c) override { return ch[c]; }
    int getChannels() override { return 2; }
    int getSize() override { return size; }
    void clear() override { ch[0].clear(); ch[1].clear(); }
};

static const int kBlock = 64;
static const float kSr = 48000.f;

int main()
{
    PatchCtrls ctrls = {};
    PatchCvs cvs = {};
    PatchState state = {};
    state.sampleRate = kSr;
    state.blockSize = kBlock;
    state.blockRate = kSr / kBlock;
    ctrls.resonatorVol = 1.f;
    ctrls.resonatorTune = 0.5f;
    ctrls.resonatorFeedback = 0.5f;

    Resonator* reso = Resonator::create(&ctrls, &cvs, &state);
    Buffer in(kBlock), out(kBlock);

    struct Phase { const char* name; float voct; float seconds; };
    const Phase phases[] = {
        {"original", 0.f, 2.f},
        {"V/Oct", 0.75f, 4.f},
        {"original again", 0.2f, 1.f},
        {"V/Oct again", 1.f, 1.f},
    };
    double cost[2] = {0, 0};
    long blocks[2] = {0, 0};
    float peak = 0;
    int bad = 0;
    srand(1);

    for (const Phase& ph : phases) {
        ctrls.resonatorVoct = ph.voct;
        bool voct = ph.voct >= kVoctModeThreshold;
        int n = (int)(ph.seconds * kSr / kBlock);
        float phasePeak = 0;
        for (int b = 0; b < n; b++) {
            float t = (float)b / n;
            // Sparse noise bursts in, so the poles ring and decay.
            for (int i = 0; i < kBlock; i++) {
                float x = (b % 200 < 3) ? (randf() * 2.f - 1.f) : 0.f;
                in.ch[0][i] = x;
                in.ch[1][i] = x * 0.7f;
            }
            ctrls.resonatorTune = 0.5f + 0.5f * sinf(t * 6.f);
            ctrls.resonatorFeedback = t;                 // up to infinite feedback at the end
            ctrls.resonatorDissonance = (b / 500) % 2 ? t : 0.f;
            cvs.resonatorVoct = (float)((b / 150) % 9) * 7.f - 12.f; // a stepped sequence
            cvs.resonatorTune = 0.3f;
            cvs.resonatorFeedback = 0.3f;

            out.ch[0].copyFrom(in.ch[0]);
            out.ch[1].copyFrom(in.ch[1]);
            auto t0 = std::chrono::steady_clock::now();
            reso->process(in, out);
            auto t1 = std::chrono::steady_clock::now();
            cost[voct] += std::chrono::duration<double>(t1 - t0).count();
            blocks[voct]++;

            for (int c = 0; c < 2; c++)
                for (int i = 0; i < kBlock; i++) {
                    float y = out.ch[c][i];
                    if (!std::isfinite(y)) bad++;
                    else phasePeak = std::max(phasePeak, fabsf(y));
                }
        }
        printf("%-15s peak %.2f\n", ph.name, phasePeak);
        peak = std::max(peak, phasePeak);
    }
    double us0 = cost[0] / blocks[0] * 1e6, us1 = cost[1] / blocks[1] * 1e6;
    printf("host time per block: original %.2f us, V/Oct %.2f us (x%.2f)\n", us0, us1, us1 / us0);
    Resonator::destroy(reso);

    if (bad || peak > 8.f) {
        printf("FAIL: %d non-finite samples, peak %.2f\n", bad, peak);
        return 1;
    }
    printf("ok\n");
    return 0;
}
