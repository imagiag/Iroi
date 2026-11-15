// Saved settings go to the firmware as MIDI pitch bend and come back as int16.
// Checks that every knob value survives a save and load, 1.0 included (the stock
// patch's encoding turned 1.0 into -1.0, which inverted CV amounts at their
// default), and that files already saved that way load as 1.0.
#include "Commons.h"
#include "MidiMessage.h"
#include <cstdio>

extern "C" void assert_failed(const char* msg, const char* location, int line) { exit(2); }
extern "C" float randf() { return rand() / (float)RAND_MAX; }

static int16_t ThroughMidi(int16_t v)
{
    MidiMessage m = MidiMessage::pb(0, v);
    return m.getPitchBend();
}

int main()
{
    int bad = 0;
    float worst = 0;
    for (int i = 0; i <= 1000; i++) {
        float v = i / 1000.f;
        float back = DecodeSetting(ThroughMidi(EncodeSetting(v)));
        float err = fabsf(back - v);
        worst = std::max(worst, err);
        if (err > 1.f / 8192.f) {
            printf("FAIL: %.4f saved, %.4f loaded\n", v, back);
            bad++;
        }
    }
    int16_t stock = ThroughMidi((int16_t)rintf(1.f * 8192));
    printf("stock encoding of 1.0 loads as %.4f raw; read now as %.4f\n", stock / 8192.f, DecodeSetting(stock));
    if (DecodeSetting(stock) != 1.f) bad++;
    printf("settings round trip: worst error %.6f\n", worst);
    if (bad) return 1;
    printf("ok\n");
    return 0;
}
