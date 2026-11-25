# Épico 7 — Monitoramento Acústico Avançado & Balística (`VU-01` a `VU-04`)

## Objetivo e Escopo

Fornecer medição acústica calibrada e balística profissional no OpenAudio Studio, alinhada às normas IEC 60268-17, Bob Katz K-System (`K-12`, `K-14`, `K-20`) e ITU-R BS.1770-4 / EBU R128.

## Escalas Suportadas (`VU-01`)

| Escala | Referência `0` | Faixa Exibida | Zona Âmbar | Zona Vermelha |
| --- | --- | --- | --- | --- |
| `dBFS Nominal` | `0.0 dBFS` | `-60 .. 0 dBFS` | `-12 .. -3 dBFS` | `> -3 dBFS` |
| `K-12` | `-12.0 dBFS RMS` | `-48 .. +12 VU` | `0 .. +4 VU` | `> +4 VU` |
| `K-14` | `-14.0 dBFS RMS` | `-46 .. +14 VU` | `0 .. +4 VU` | `> +4 VU` |
| `K-20` | `-20.0 dBFS RMS` | `-40 .. +20 VU` | `0 .. +4 VU` | `> +4 VU` |
| `EBU R128 LUFS` | `-23.0 LUFS` (`0 LU`) | `-59 .. -5 LUFS` | `-23 .. -14 LUFS` | `> -14 LUFS` |

## Balística IEC 60268-17 & Peak Hold (`VU-02`)

- **Ataque:** instantâneo (`new_level >= current_level`).
- **Decaimento:** `20.0 dB/s` (`current - 20.0f * dt_seconds`).
- **Integração RMS:** constante de tempo de `300 ms` (`alpha = 1 - exp(-dt / 0.300)`).
- **Peak Hold:** retenção por `1.2 s` após novo pico antes de iniciar decaimento a `20.0 dB/s`.

## Processador ITU-R BS.1770 / EBU R128 (`VU-03`)

- **Filtro K-Weighting:** cascata de dois biquads por canal (Estágio 1: High-Shelf `+4 dB` em ~1682 Hz; Estágio 2: High-Pass de 2ª ordem para atenuação de sub-graves).
- **Janelas de Loudness:** blocos de `100 ms` com sobreposição de `75%`:
  - **Momentary (`M`):** `400 ms` (4 blocos).
  - **Short-Term (`S`):** `3 s` (30 blocos).
  - **Integrated (`I`):** média com *gating* duplo: gate absoluto em `-70.0 LUFS` seguido de gate relativo em `L_ungated - 10.0 LU`.
- **True Peak (`dBTP`):** interpolador polifásico FIR `4x` (48 taps, 12 taps por fase conforme ITU-R BS.1770-4 Anexo 2) capaz de detectar picos inter-amostra.

## Calibração Acústica (`VU-04`)

- Uma senoide a `1 kHz` com nível RMS de `-14.0 dBFS` marca `0.00 VU (±0.05 dB)` na escala `K-14`.
- Analogamente, `-12.0 dBFS RMS` marca `0.00 VU (±0.05 dB)` em `K-12` e `-20.0 dBFS RMS` marca `0.00 VU (±0.05 dB)` em `K-20`.

## Validação

Executar `./scripts/build_studio.ps1 -Config Release` (e `-Config Debug`) com o smoke test `--smoke-vu`, validando `VU-01` a `VU-04`.
