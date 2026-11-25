# Design

## Context

`AutomationLaneComponent::read_points` usa `automations[0]`. O arraste e a inserção no duplo clique também escrevem no índice zero. O Inspector filtra curvas temporais e usa o domínio do parâmetro. O modelo e o compilador já distinguem as origens por `is_timeline` e os alvos por `target`. A change `fix-studio-track-automation-toggle` tem os aceites de abertura, recolhimento e geometria.

## Goals / Non-Goals

**Objetivos.** Usar a mesma regra de seleção no desenho, na detecção de pontos e nas mutações. Preservar outras automações e documentar a diferença entre os domínios da Timeline e do Inspector.

**Limites.** A faixa continua dedicada a volume temporal. A change cobre Studio e manual. Formato de projeto e banco, reprodução, fades de clipe e reparo de dados previamente alterados ficam fora do escopo.

## Decisions

- Na pista do evento corrente, localizar a primeira entrada com `is_timeline: true` e `target: volume`. Se houver mais de uma curva temporal de volume, editar a primeira compatível preserva uma seleção determinística no editor de faixa única e conserva as demais entradas.
- Usar essa seleção na leitura e revalidar o destino ao gravar pelo `AuthoringModel`. Remover a escrita de fallback no índice zero. Quando a faixa recebe o primeiro ponto e ainda não existe entrada compatível, anexar uma curva temporal de volume em uma operação de autoria com Undo/Redo.
- Preservar os gestos de edição e a geometria do botão `A`. No manual, tratar `Volume Automation` como volume ao longo do tempo no capítulo 5 e explicar `TRACK PARAM CURVES`, `+ Add Curve` e os alvos `volume`, `pitch` e `pan` no capítulo 6.

## Risks / Trade-offs

- Tornar a leitura correta sem mudar a escrita ainda permite alterar outra curva. O smoke deve comparar os dados das curvas vizinhas após arraste, duplo clique e Undo/Redo, nas duas ordens.
- Uma pista contendo somente curvas por parâmetro deve abrir com a mesma grade de uma pista vazia. O teste de criação deve exercitar o duplo clique com curvas já cadastradas, pois o fallback atual depende de o array estar vazio.
