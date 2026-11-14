// Host test: runs the real Pole (Resonator.h) on the computer, pings it with an
// impulse in V/Oct mode and measures the pitch it rings at.
//   make -C test        (prints a table, exits 1 if any note is off by more than the limit)
#define private public // the naive comparison pokes at Pole's delay
#include "Resonator.h"
#undef private
#include <cstdio>
#include <vector>

// OWL's message.h hook, not linked on the computer.
extern "C" void assert_failed(const char* msg, const char* location, int line)
{
    fprintf(stderr, "assert failed: %s %s:%d\n", msg, location, line);
    exit(2);
}

extern "C" float randf() { return rand() / (float)RAND_MAX; }

static const float kSr = 48000.f;
static const size_t kBlock = 64;

// Feedback settings as Resonator::SetFeedback applies them to every pole.
static void ApplyFeedback(Pole* p, float value)
{
    p->SetFeedback(Map(value, 0.f, 1.f, 0.85f, 1.f));
    p->SetReso(Map(value, 0.f, 1.f, 0.5f, 0.6f));
    p->SetFilter(Map(value, 0.f, 1.f, 5000.f, 10000.f));
}

// Coarse fundamental by autocorrelation around the expected period.
static float CoarseHz(const std::vector<float>& x, float expectHz)
{
    float period = kSr / expectHz;
    int lo = (int)(period * 0.75f), hi = (int)(period * 1.3f) + 2;
    size_t n = std::min(x.size() - hi - 1, (size_t)(period * 8));
    std::vector<double> r(hi + 2, 0.0);
    for (int lag = lo - 1; lag <= hi + 1; lag++) {
        double acc = 0;
        for (size_t i = 0; i < n; i++) acc += (double)x[i] * x[i + lag];
        r[lag] = acc;
    }
    int best = lo;
    for (int lag = lo; lag <= hi; lag++) if (r[lag] > r[best]) best = lag;
    double a = r[best - 1], b = r[best], c = r[best + 1];
    return kSr / (float)(best + 0.5 * (a - c) / (a - 2 * b + c));
}

// Frequency of the partial near hz, from the phase advance between two
// adjacent Hann windows of the demodulated signal. Unlike an autocorrelation
// peak this is not biased by the decay of the ringing.
static float PartialHz(const std::vector<float>& x, float hz, float f0)
{
    size_t w = (size_t)(6.f * kSr / f0);
    if (2 * w > x.size()) w = x.size() / 2;
    double re[2] = {0, 0}, im[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        for (size_t i = 0; i < w; i++) {
            size_t n = k * w + i;
            double win = 0.5 - 0.5 * cos(2 * M_PI * i / w);
            double ph = 2 * M_PI * hz * n / kSr;
            re[k] += win * x[n] * cos(ph);
            im[k] -= win * x[n] * sin(ph);
        }
    }
    double dphi = atan2(im[1], re[1]) - atan2(im[0], re[0]);
    while (dphi > M_PI) dphi -= 2 * M_PI;
    while (dphi < -M_PI) dphi += 2 * M_PI;
    return hz + (float)(dphi * kSr / (2 * M_PI * w));
}

struct Pitch { float f1, h3; }; // fundamental partial, 3rd partial / 3

static Pitch Measure(const std::vector<float>& x, float expectHz)
{
    float c = CoarseHz(x, expectHz);
    Pitch p;
    p.f1 = PartialHz(x, c, c);
    p.h3 = PartialHz(x, 3.f * c, c) / 3.f;
    return p;
}

static float Cents(float hz, float ref) { return 1200.f * log2f(hz / ref); }

// mode 0: compensated (the firmware), 1: naive delay = sr/f, for comparison.
static Pitch RunNote(float note, float feedback, int mode)
{
    Pole* p = Pole::create(kSr);
    ApplyFeedback(p, feedback);
    if (mode == 0) {
        p->SetVoctNotes(note, note, kBlock);
    } else {
        // Naive: the old-style delay with no filter compensation.
        p->SetVoctNotes(69.f, 69.f, kBlock); // anything, overwritten below
    }
    // Let the delay glide settle (one block), with silence.
    for (size_t i = 0; i < kBlock; i++) p->Process(0.f, LEFT_CHANNEL);

    std::vector<float> out;
    size_t len = (size_t)(kSr * 0.5f);
    out.reserve(len);
    for (size_t i = 0; i < len; i++) {
        if (mode == 1 && i == 0) {
            p->delayTimes_[LEFT_CHANNEL] = kSr / voct::NoteToHz(note);
            p->delayIncs_[LEFT_CHANNEL] = 0;
        }
        // The firmware retunes every block (Resonator::process).
        if (mode == 0 && i % kBlock == 0) p->SetVoctNotes(note, note, kBlock);
        float in = (i == 0) ? 1.f : 0.f;
        out.push_back(p->Process(in, LEFT_CHANNEL));
    }
    Pole::destroy(p);
    // Skip the first few periods (transient).
    float skip = 4.f * kSr / voct::NoteToHz(note);
    std::vector<float> tail(out.begin() + (size_t)std::min(skip, len * 0.3f), out.end());
    return Measure(tail, voct::NoteToHz(note));
}

int main()
{
    const float limit = 3.f;     // cents, for the fundamental...
    const float checkedUpTo = 91.f; // ...up to G6 (1.57 kHz); above, low feedback rings too briefly to matter
    const float feedbacks[] = {0.f, 0.5f, 1.f};
    int fails = 0;
    float worst = 0;
    printf("Pole in V/Oct mode, pinged; cents off the note.\n");
    printf("note     Hz   | fundamental @ feedback 0 / .5 / 1 | 3rd partial/3 @ .5 | naive sr/f @ .5\n");
    for (float note = 16.f; note <= 103.f; note += 3.f) {
        float hz = voct::NoteToHz(note);
        printf("%4.0f %8.1f  |", note, hz);
        Pitch mid = {0, 0};
        for (float fb : feedbacks) {
            Pitch p = RunNote(note, fb, 0);
            if (fb == 0.5f) mid = p;
            float c = Cents(p.f1, hz);
            printf(" %+7.2f", c);
            if (note <= checkedUpTo) {
                worst = std::max(worst, fabsf(c));
                if (fabsf(c) > limit) fails++;
            }
        }
        printf("          | %+7.2f            | %+7.1f\n", Cents(mid.h3, hz), Cents(RunNote(note, 0.5f, 1).f1, hz));
    }
    printf("\nworst fundamental error up to note %.0f: %.2f cents (limit %.1f), %d over the limit\n", checkedUpTo, worst, limit, fails);
    return fails ? 1 : 0;
}
