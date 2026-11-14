// Renders the Resonator in its original mode to a raw float file, using only
// the stock parameters. Built against this tree and against the stock source
// (see `make regression`) to check that the original mode is unchanged.
#include "Resonator.h"
#include <cstdio>

extern "C" void assert_failed(const char* msg, const char* location, int line) { exit(2); }
extern "C" float randf() { return rand() / (float)RAND_MAX; }
extern "C" float fast_powf(float x, float y) { return (powf)(x, y); }
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

int main(int argc, char** argv)
{
    const int kBlock = 64;
    static PatchCtrls ctrls;   // zeroed (stock code has no V/Oct field: 0 = original mode)
    static PatchCvs cvs;
    static PatchState state;
    state.sampleRate = 48000.f;
    state.blockSize = kBlock;
    ctrls.resonatorVol = 1.f;
    ctrls.resonatorTuneCvAmount = 0.5f;
    ctrls.resonatorFeedbackCvAmount = 0.5f;

    Resonator* reso = Resonator::create(&ctrls, &cvs, &state);
    Buffer in(kBlock), out(kBlock);
    FILE* f = fopen(argv[1], "wb");
    srand(1);
    for (int b = 0; b < 3000; b++) {
        float t = b / 3000.f;
        for (int i = 0; i < kBlock; i++) {
            float x = (b % 100 < 2) ? (randf() * 2.f - 1.f) : 0.f;
            in.ch[0][i] = x;
            in.ch[1][i] = -x;
        }
        // Start where the stock code's uninitialised tuning state doesn't matter.
        ctrls.resonatorTune = b < 10 ? 0.f : 0.5f + 0.45f * sinf(t * 9.f);
        ctrls.resonatorFeedback = t;
        ctrls.resonatorDissonance = 0.5f - 0.5f * cosf(t * 4.f);
        cvs.resonatorTune = 0.2f * sinf(t * 23.f);
        cvs.resonatorFeedback = 0.1f;
        reso->process(in, out);
        if (b >= 10) {
            fwrite((float*)out.ch[0], sizeof(float), kBlock, f);
            fwrite((float*)out.ch[1], sizeof(float), kBlock, f);
        }
    }
    fclose(f);
    return 0;
}
