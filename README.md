# snn-nibble — a neuromorphic (spiking) Iris accelerator, v1 + improved v2

The **neuromorphic archetype** (Loihi / ODIN direction) of the coheratech accelerator study:
**integrate-and-fire spiking neurons** with deterministic rate-coded inputs. Weights AND
per-neuron thresholds are **runtime-loaded**; the computation class is spikes-per-timestep,
not MACs-on-values, so (like the DPU row) the comparison to the other columns is at the
**accuracy level** — while the engine itself is fully integer and deterministic, with its own
reproducible golden checksum (`0xea6d8757`, established in csim, reproduced on the board).

## The conversion (host-side, becomes runtime config)

- **Signed rate coding**: rate = |xq − zp|/256 via a deterministic Bresenham phase
  accumulator; spikes carry sign — the **inhibitory rail matters**: iris features go far below
  the −17 zero-point (to −128), and unsigned coding silently zeroes them (accuracy collapsed
  to ~110/150 before this fix).
- **Thresholds from the requant scales**: θ = 256/scale (scale = m0/2³¹/2^shift per channel),
  so hidden/output spike rates mirror the quantized MLP's activations; per-step bias currents
  make the expected per-step input equal the MLP's accumulator exactly.
- **ReLU-equivalent IF neurons**: reset-by-subtraction, no membrane floor (a floor causes
  rectification errors — spurious spikes from negative-mean-current neurons).
- **T = 128 timesteps** suffices for the full **147/150** (matches the int8 MLP exactly).

## Results — BOARD-MEASURED (`xczu7ev-ffvc1156-2-e`)

| variant | engine | clock (closed) | per-inference | accuracy |
|---|---|---|---|---|
| **v1** | one neuron population, II=2 timestep loop | 100 MHz (WNS +4.77) | **2.66 µs/inf** (376 k inf/s) | **147/150**, cs `0xea6d8757` ×5 runs |
| **v2 improved** | **4 sample-parallel populations** + clock | 150 MHz (WNS +0.34) | **0.495 µs/inf** (2.02 M inf/s) | identical, deterministic |

**v2 = 5.37× v1** (3.58× population parallelism × 1.5× clock). Rate coding costs T× the
underlying MAC work — the honest price of the spiking paradigm on a dense workload; its wins
(event-driven sparsity, on-chip STDP learning) live elsewhere. On-chip SDSP/STDP training is
the natural next step (ODIN does it in silicon); out of scope here.

## Layout
- `firmware/snn.cpp`, `firmware/snn_v2.cpp` — the two engines (HLS)
- `host/snn_config.h` — conversion: quantized MLP → weights/biases/thresholds config
- `host/snn_host.c` — board host; `tb/snn_tb.cpp` — csim gate + T sweep (plain g++)
- `deploy_snn.sh <bit> <tag> <mhz>`; `results/` — board logs + json for both variants
