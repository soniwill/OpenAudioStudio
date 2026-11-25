# Índice do legado de especificações

Este diretório contém documentos congelados para consulta durante a migração incremental para OpenSpec. Requisitos vigentes de capacidades migradas ficam em `openspec/specs/`. Proponha alterações em [openspec/changes](../../openspec/changes/). Os números no mapa indicam as seções do [blueprint consolidado](../legacy/blueprint.md).

## Mapa das 19 capacidades

As capacidades de conceito refletem os conceitos do FMOD Studio; as capacidades de plataforma descrevem mecanismos transversais.

| Tipo | Capacidade OpenSpec | Seções pertinentes do blueprint |
|---|---|---|
| Conceito | `events` | §4.3 EVTS; §7 Épico 2; §8 T08 |
| Conceito | [`instruments`](../../openspec/specs/instruments/spec.md) | Migrado para `openspec/specs/instruments/spec.md`. Legado: §4.3 EVTS/SMPL/DATA; §7 Épicos 2 e 9; §8 T08, T18–T25 |
| Conceito | [`timeline-logic`](../../openspec/specs/timeline-logic/spec.md) | Migrado para `openspec/specs/timeline-logic/spec.md`. Legado: §3.3, §3.7–3.8; §4.3 LOGC; §7 Épico 2; §8 T09 |
| Conceito | [`parameters`](../../openspec/specs/parameters/spec.md) | Migrado para `openspec/specs/parameters/spec.md`. Legado: §4.3 PARM; §7 Épico 2; §8 T10a |
| Conceito | [`modulation-automation`](../../openspec/specs/modulation-automation/spec.md) | Migrado para `openspec/specs/modulation-automation/spec.md`. Legado: §3.4, §3.7; §4.3 PARM; §7 Épicos 2 e 9; §8 T10b, T21 |
| Conceito | `mixing` | §3.1–3.2, §3.5–3.6; §4.3 BUSS; §7 Épicos 1, 7 e 9; §8 T04, T07, T18–T19 |
| Conceito | `snapshots-vcas` | §4.3 SNAP/VCAS; §7 Épico 2; §8 T11 |
| Conceito | [`spatialization`](../../openspec/specs/spatialization/spec.md) | Migrado para `openspec/specs/spatialization/spec.md`. Legado: §7 Épico 10; §8 T15, Épico 10 |
| Conceito | `banks-assets` | §4; §7 Épico 3; §8 T12a–T14c |
| Conceito | `live-update` | §7 Épico 8; §8 LIVE-01–LIVE-04 |
| Conceito | `metering-profiling` | §3.5; §7 Épico 7; §8 T07, VU-01–VU-04 |
| Plataforma | `engine-core` | §2–3; §7 Épicos 1–2; §8 T01–T11 |
| Plataforma | `c-api-abi` | §2.2–2.3, §5; §7 Épico 3; §8 T14a–T14c |
| Plataforma | `bank-format` | §4.1–4.4; §7 Épico 3; §8 T12a–T12b |
| Plataforma | `project-publish` | §4.4–4.5; §7 Épicos 3 e 5; §8 T13, T16b, T16f |
| Plataforma | [`studio-shell`](../../openspec/specs/studio-shell/spec.md) | Migrado para `openspec/specs/studio-shell/spec.md`. Legado: §7 Épico 5; §8 T16a–T16f |
| Plataforma | `unity-bridge` | Migração iniciada em [openspec/changes/add-unity-audio-playground](../../openspec/changes/add-unity-audio-playground/specs/unity-bridge/spec.md). Legado: §7 Épico 4; §8 T15a–T15b |
| Plataforma | [`mcp-automation`](../../openspec/specs/mcp-automation/spec.md) | Migrado para `openspec/specs/mcp-automation/spec.md`. Legado: §7 Épico 6; §8 MCP-01–MCP-05 |
| Plataforma | [`dev-harness`](../../openspec/specs/dev-harness/spec.md) | Migrada. O blueprint registra o contexto histórico nas seções §1, §6 e §9–10. |

## Documentos legados

Cada arquivo listado permanece no estado **Legado — aguardando migração**. A capacidade indicada é o destino primário de rastreabilidade; documentos que atravessam domínios listam todas as capacidades pertinentes.

| Documento legado | Capacidade(s) alvo | Status |
|---|---|---|
| `bank-v1-clarifications-proposal.md` | `bank-format`, `banks-assets` | Legado — aguardando migração |
| `epic-12-modulation-automation-ui.md` | `modulation-automation`, `studio-shell` | Legado — aguardando migração |
| `epic-7-vu-metering.md` | `metering-profiling`, `mixing` | Legado — aguardando migração |
| `epic-8-live-linking.md` | `live-update`, `unity-bridge`, `studio-shell` | Legado — aguardando migração |
| `epic-9-advanced-interactive-audio.md` | `instruments`, `timeline-logic`, `modulation-automation`, `mixing`, `banks-assets`, `unity-bridge` | Legado — aguardando migração |
| `epic-studio-pro-ui.md` | `studio-shell`, `events`, `instruments`, `mixing` | Legado — aguardando migração |
| `epic-v1-audio-2d.md` | `spatialization`, `unity-bridge`, `instruments`, `banks-assets`, `metering-profiling` | Legado — aguardando migração |
| `mcp-server-guide.md` | `mcp-automation`, `studio-shell` | Parcialmente migrado para [`mcp-automation`](../../openspec/specs/mcp-automation/spec.md) (`studio_ui_screenshot`); demais contratos aguardam migração |
| `project-json-v1-proposal.md` | `project-publish`, `bank-format`, `banks-assets` | Legado — aguardando migração |
| `project-v1.schema.proposed.json` | `project-publish`, `bank-format`, `banks-assets` | Legado — aguardando migração |
| `publication-recovery-proposal.md` | `project-publish`, `studio-shell` | Legado — aguardando migração |
| `spsc-queue.md` | `engine-core`, `c-api-abi` | Legado — aguardando migração |
| `studio-playlist-delay.md` | `instruments`, `timeline-logic`, `studio-shell` | Legado — aguardando migração |
| `system-api.md` | `c-api-abi`, `engine-core` | Legado — aguardando migração |
| `T01-plan.md` | `dev-harness`, `engine-core` | Legado — aguardando migração |
| `T03-sound-contract.md` | `engine-core`, `banks-assets`, `instruments` | Legado — aguardando migração |
| `T04-bus-contract.md` | `mixing`, `engine-core` | Legado — aguardando migração |
| `T05-voice-contract.md` | `engine-core`, `instruments`, `metering-profiling` | Legado — aguardando migração |
| `T06-timing-contract.md` | `engine-core`, `timeline-logic` | Legado — aguardando migração |
| `T07-effects-contract.md` | `mixing`, `metering-profiling` | Legado — aguardando migração |
| `T08-studio-events-contract.md` | `events`, `instruments`, `parameters` | Legado — aguardando migração |
| `T09-timeline-contract.md` | `timeline-logic`, `events` | Parcialmente migrado para [`timeline-logic`](../../openspec/specs/timeline-logic/spec.md) (autoria; paridade de transições em runtime em andamento) |
| `T10a-parameters-contract.md` | `parameters`, `events` | Legado — aguardando migração |
| `T10b-modulators-automation-contract.md` | `modulation-automation`, `parameters` | Legado — aguardando migração |
| `T11-snapshots-vcas-contract.md` | `snapshots-vcas`, `mixing` | Legado — aguardando migração |
| `T12-bank-io-contract.md` | `bank-format`, `banks-assets`, `engine-core` | Legado — aguardando migração |
| `T13-bank-compiler-contract.md` | `project-publish`, `bank-format`, `banks-assets` | Legado — aguardando migração |
| `T14-api-hardening-contract.md` | `c-api-abi`, `engine-core`, `banks-assets` | Legado — aguardando migração |
| `T15-unity-package-contract.md` | `unity-bridge`, `c-api-abi`, `banks-assets` | Legado — aguardando migração |
| `T16-desktop-studio-contract.md` | `studio-shell`, `project-publish` | Legado — aguardando migração |
| `T16-portable-projects-contract.md` | `project-publish`, `studio-shell`, `banks-assets` | Legado — aguardando migração |
| `T16c-authoring-model-inspector-contract.md` | `studio-shell`, `events`, `instruments`, `parameters`, `snapshots-vcas`, `mixing` | Legado — aguardando migração |
| `T16d-timeline-waveforms-contract.md` | `studio-shell`, `timeline-logic`, `instruments`, `modulation-automation` | Legado — aguardando migração |
| `T16e-audition-transport-mixer-contract.md` | `studio-shell`, `mixing`, `events`, `parameters`, `snapshots-vcas` | Legado — aguardando migração |
| `T16f-packaging-end-to-end-contract.md` | `studio-shell`, `project-publish`, `unity-bridge`, `banks-assets` | Legado — aguardando migração |
| `V1-acceptance-guide.md` | `unity-bridge`, `spatialization`, `banks-assets`, `metering-profiling` | Legado — aguardando migração |

O índice cobre os documentos Markdown e o schema JSON existentes neste diretório. Ao migrar um contrato, atualize o índice com o caminho do spec canônico.
