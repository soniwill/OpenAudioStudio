# Design

## Context

Ver `proposal.md — Why`. Estado atual relevante:

- `AutomationLaneComponent` trata apenas mouse (`mouseDown` insere por duplo clique e seleciona para arraste, `mouseDrag`, `mouseUp`, `mouseMove`, `mouseExit`). Não define `keyPressed`, não pede foco e não tem menu de contexto.
- `TimelineRuler` é o único componente da timeline que faz `setWantsKeyboardFocus(true)` e `grabKeyboardFocus()` no clique. `TimelinePanel::keyPressed` remove o instrumento selecionado em `Delete`/`Backspace`. Por isso a tecla vaza para o clipe quando a interação anterior deixou o foco na régua.
- `AuthoringModel::set_track_timeline_volume_points` rejeita lista vazia e substitui somente a curva `target=volume`, `is_timeline=true`, preservando as demais entradas.
- `AutomationCurveEditor` já remove pontos com botão direito, mas exige manter ao menos um ponto.
- Runtime: `curve_eval` devolve `0.0f` para curva vazia (`native/src/common/curve.h:38`); o parser do banco aceita lista de pontos vazia (`native/tools/bankc/project_parser.h:382`). Não há dependência de engine a alterar.

## Goals / Non-Goals

**Goals:**

- Excluir um ponto individual pela tecla e pelo menu de contexto, com um passo único de Undo/Redo.
- Remover a curva temporal de volume quando o último ponto for excluído, sem afetar outras curvas.
- Fazer o foco da timeline seguir o último alvo clicado, para que `Delete` atue sobre o que o usuário acabou de tocar.

**Non-Goals:**

- Alterar inserção, arraste, formas de segmento, domínio de valores ou o editor do Inspector.
- Introduzir atalhos novos além de `Delete`/`Backspace`.
- Alterar formato de banco, parser, runtime ou a ABI.

## Decisions

### 1. A faixa trata a tecla, não o `TimelinePanel`

`AutomationLaneComponent` ganha `setWantsKeyboardFocus(true)`, `keyPressed` e `grabKeyboardFocus()` no `mouseDown`. Assim a tecla chega ao componente que possui o estado do ponto selecionado.

Alternativa descartada: fazer `TimelinePanel::keyPressed` consultar as pistas para saber se há ponto selecionado. O painel passaria a depender do estado interno de cada faixa e continuaria sem resolver a ambiguidade de foco.

### 2. Foco segue o último alvo clicado (faixa ou clipe)

`ClipComponent` também passa a pedir foco e a capturá-lo no `mouseDown`. A faixa limpa o ponto selecionado ao perder o foco.

Sem isso, o foco permaneceria na faixa depois de clicar em um clipe e `Delete` excluiria o ponto antigo em vez do clipe. Alternativa descartada: não consumir a tecla quando não houver ponto — o estado de seleção ficaria obsoleto e continuaria a misrotear a tecla.

### 3. `Delete` é consumido quando a faixa está focada

Com ponto selecionado (ou sob o ponteiro, como fallback), a faixa exclui o ponto e retorna `true`. Sem ponto, retorna `true` sem alterar nada. A tecla nunca continua para `TimelinePanel` a partir da faixa.

Alternativa descartada: devolver `false` sem ponto selecionado para permitir exclusão de clipe; reintroduz o misroteamento descrito na decisão 2.

### 4. Excluir o último ponto remove a entrada da curva

Novo `AuthoringModel::clear_track_timeline_volume_points(event_path, track_index)` reaproveita o localizador da curva temporal e apaga apenas aquela entrada do array `automations`, via `modify`, preservando ordem e conteúdo das demais.

Alternativas descartadas:

- **Manter ≥1 ponto (opção A)**: exigiria um no-op silencioso no último ponto, sem feedback na faixa.
- **Aceitar lista vazia em `set_track_timeline_volume_points`**: a assinatura `bool` ficaria ambígua entre "rejeitado" e "removido", e o projeto passaria a conter curva sem pontos.

Não há violação do invariante de 1 a 32 pontos de `modulation-automation`: a curva deixa de existir em vez de ser armazenada vazia.

### 5. Menu de contexto como caminho descobrível

O `mouseDown` com `isPopupMenu()` sobre um ponto abre um menu com a opção de exclusão, seguindo o precedente do `AutomationCurveEditor`, porém embrulhado em `begin_gesture`/`end_gesture` para manter o passo único de Undo/Redo.

### 6. Índice de ponto validado contra o estado atual

Antes de excluir, a faixa relê os pontos e valida o índice. Edições externas (Inspector, MCP, undo) podem ter mudado a lista desde a seleção; um índice inválido resulta em no-op.

## Risks / Trade-offs

- [Foco no clipe altera o roteamento de outras teclas] → O componente não trata teclas; o evento continua subindo para `TimelinePanel` e `MainComponent` como hoje. A cobertura do smoke inclui `Ctrl+Z` após exclusão com foco na faixa.
- [Seleção de ponto obsoleta após edição externa] → Validar o índice no momento da exclusão e limpar a seleção em `focusLost`.
- [Remoção da curva durante a reprodução] → A alteração passa pelo mesmo caminho de `modify` das demais edições de autoria; o runtime já trata ausência da curva como `0.0`. Verificar por MCP em prévia ativa.
- [Teste headless sem foco real] → O smoke chama `keyPressed` diretamente no componente e verifica foco por `hasKeyboardFocus` apenas quando o ambiente permitir; o critério de aceite permanece o efeito nos dados e no passo de Undo.

## Migration Plan

Nenhuma migração de dados. Projetos existentes mantêm as curvas; a entrada só desaparece quando o usuário exclui o último ponto, e `Ctrl+Z` restaura. Sem alteração de banco, ABI ou versão de projeto.

## Open Questions

Nenhuma pendente.
