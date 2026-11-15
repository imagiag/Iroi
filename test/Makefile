# Host tests: compile the patch's DSP headers for this computer and check them.
#   make            build and run the V/Oct tracking test, the resonator smoke test
#                   and the saved-settings round trip
#   make regression check the original resonator mode against the stock Befaco source
#                   (git tag `stock`), sample for sample
OWL = ../lib/OwlProgram
BASEFLAGS = -std=gnu++17 -O2 -w -Dexp10f=__exp10f -I$(OWL)/LibSource -I$(OWL)/Source
CXXFLAGS = -I.. $(BASEFLAGS)

all: out/voct_tracking out/resonator_smoke out/settings_roundtrip
	./out/voct_tracking
	./out/resonator_smoke
	./out/settings_roundtrip

out/voct_tracking: voct_tracking.cpp ../Resonator.h ../VoctPitch.h ../Commons.h ../DelayLine.h
	@mkdir -p out
	$(CXX) $(CXXFLAGS) -o $@ voct_tracking.cpp $(OWL)/LibSource/FloatArray.cpp

out/resonator_smoke: resonator_smoke.cpp ../Resonator.h ../VoctPitch.h ../Commons.h ../DelayLine.h
	@mkdir -p out
	$(CXX) $(CXXFLAGS) -o $@ resonator_smoke.cpp $(OWL)/LibSource/FloatArray.cpp

out/settings_roundtrip: settings_roundtrip.cpp ../Commons.h
	@mkdir -p out
	$(CXX) $(CXXFLAGS) -o $@ settings_roundtrip.cpp $(OWL)/LibSource/FloatArray.cpp

STOCK = out/stock-src
regression: render_original.cpp
	@mkdir -p out
	rm -rf $(STOCK) && mkdir -p $(STOCK) && git -C .. archive stock | tar -x -C $(STOCK)
	$(CXX) $(CXXFLAGS) -o out/render_new render_original.cpp $(OWL)/LibSource/FloatArray.cpp
	$(CXX) -I$(STOCK) $(BASEFLAGS) -o out/render_stock render_original.cpp $(OWL)/LibSource/FloatArray.cpp
	./out/render_new out/new.f32 && ./out/render_stock out/stock.f32
	cmp out/new.f32 out/stock.f32 && echo "original mode: identical to stock"

clean:
	rm -rf out

.PHONY: all clean regression
