# tests/signallab

Tests of Signal Lab (test_signallab.cpp): every stage alone, then the pipeline end to end on generated signals. `quick` is the ctest `signallab`; `gate` is the slow ctest `signallab_eval` (36 random signals, each stage must stay above its floor). `evaluate N [seed]` prints the per-stage report for N random signals, `case N` one of them with the pipeline's log, `pipe N` one of the fixed pipeline cases.
