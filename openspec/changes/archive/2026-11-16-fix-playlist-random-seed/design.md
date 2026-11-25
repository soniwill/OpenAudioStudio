# Design

## Context

O `PlaylistState` global nasce com `Pcg32(0x12345678, 0x9abcdef0)` (`native/src/core/studio_model.h`) e cada `oae_event_play` cria uma nova `EventDescription` (`native/src/api/event_api.cpp`), recriando o estado com a mesma semente. A primeira escolha de cada play é, portanto, sempre idêntica. Ver o bug 2 em `../studio-help-workflow-accuracy/evidence/FINDINGS-1.4.md`.

## Goals / Non-Goals

**Goals:**
- Sorteios de plays distintos variarem entre si e convergirem à distribuição de pesos.
- Preservar determinismo *dentro* de uma instância (modos como `RandomNoRepeat` e o atraso estocástico continuam previsíveis na mesma instância).
- Manter gates existentes estáveis (testes com sorteio não podem virar flaky).

**Non-Goals:**
- Mudar modos de playlist, escopo (Global/Instance) ou o formato `.bank`.
- Semeadura configurável pelo usuário ou API de seed por instância.

## Decisions

1. **Semear no ponto de criação do estado, não em cada sorteio.** O gerador continua PCG32; apenas a semente inicial deixa de ser constante. Alternativa considerada: semear por `EventInstance` — descartada porque o escopo Global (compartilhado entre instâncias) é justamente o comportamento compilado no banco e não deve mudar.
2. **Fonte de entropia: contador global atômico misturado com `std::chrono::steady_clock` (ou `random_device` quando disponível) como semente de um gerador central.** Alternativa considerada: `random_device` direto por estado — descartada para não multiplicar chamadas ao SO; um único gerador central não determinístico semeia cada `PlaylistState`, mantendo o custo por play desprezível.
3. **Teste estatístico com margem, não igualdade exata.** O teste nativo faz N plays independentes de uma playlist 80/20 e exige: (a) ambas as entradas selecionadas ao menos uma vez e (b) frequência da entrada de 80% dentro de uma faixa tolerante (ex.: 3 desvios padrão binomiais). Isso valida o requisito sem acoplar o teste a valores individuais de sorteio.

## Risks / Trade-offs

- [Gates existentes que assumem primeira escolha fixa podem quebrar] → Buscar por valores de semente `0x12345678` em testes e ajustá-los para dependerem de comportamento estatístico ou de instância única.
- [Entropia indisponível em plataformas restritas] → Fallback determinístico: contador atômico sempre crescente já varia entre plays; `steady_clock`/`random_device` são melhora, não dependência dura.
- [Convergência estatística lenta com poucas entradas] → O requisito exige variação e ambas as entradas, não proporção exata por play; a margem cobre isso.
