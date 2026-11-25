# Spec Delta: studio-shell

## MODIFIED Requirements

### Requirement: Alternância entre modos minimizado e maximizado via grip lateral
O Studio SHALL permitir alternar os docks de barra entre o modo Minimizado (Compacto) e o modo Maximizado (Completo) por duplo-clique no grip lateral ou via menu contextual acionado com o botão direito do mouse. O modo minimizado do transporte SHALL ocultar o botão de atalho de parâmetros e o status extenso, exibindo apenas controles básicos de reprodução e o timecode.

#### Scenario: Duplo-clique no grip lateral alterna para modo compacto
- **WHEN** o usuário executa um duplo-clique sobre a área do grip lateral da `Transport Bar` maximizada
- **THEN** a barra recolhe o status secundário e o botão de acesso a parâmetros
- **AND** a barra permanece visível no topo ocupando largura reduzida com botões essenciais e timecode

#### Scenario: Menu de contexto no grip lateral com botão direito
- **WHEN** o usuário clica com o botão direito do mouse sobre o grip lateral de uma barra de ferramentas
- **THEN** o Studio exibe um menu popup com opções para alternar entre modo compacto e expandido e para alternar entre janela flutuante e encaixe no topo

## ADDED Requirements

### Requirement: Fita Retrátil de Parâmetros Contextuais na Timeline
O `TimelinePanel` SHALL fornecer uma fita horizontal retrátil no topo da área de edição exibindo exclusivamente os parâmetros locais e globais consumidos pelo evento ativo. A fita SHALL recolher automaticamente para altura zero quando o evento não consome parâmetros e permitir expansão ou colapso manual pelo usuário.

#### Scenario: Exibição contextual de parâmetros vinculados ao evento
- **WHEN** um evento contendo parâmetros locais ou referências a parâmetros globais em automações, lógica ou sheets é aberto na Timeline
- **THEN** a fita exibe sliders com caixas de leitura de valor numérico legíveis e identificação nominal de cada parâmetro
- **AND** duplo-clique sobre qualquer slider restaura o respectivo valor padrão (`default_value`)

#### Scenario: Recolhimento automático para eventos sem parâmetros
- **WHEN** o evento aberto na Timeline não consome nenhum parâmetro local ou global
- **THEN** a fita de parâmetros permanece recolhida com altura zero
- **AND** a régua de tempo e as faixas de áudio ocupam toda a área vertical útil da Timeline

#### Scenario: Parâmetro Labeled exibe texto do estado na fita
- **WHEN** um parâmetro do tipo `labeled` consumido pelo evento é apresentado na fita da Timeline
- **THEN** o controle exibe o rótulo textual do valor atual em vez de um índice numérico puro

### Requirement: Deck de Parâmetros Globais no Bottom Deck e Desacoplamento Flutuante
O `ContextualBottomDeck` SHALL disponibilizar uma aba dedicada de Parâmetros Globais contendo busca textual, filtros e um grid de cards responsivo com controles específicos para parâmetros contínuos, discretos e rotulados. O deck SHALL permitir desacoplamento em janela flutuante com suporte ao retorno via ação Dock Back.

#### Scenario: Grid responsivo de cards com controles especializados
- **WHEN** o usuário seleciona a aba `Parameters` no Bottom Deck
- **THEN** o Studio renderiza os parâmetros globais em colunas de cards com faders largos para contínuos, passos para discretos e botões segmentados com textos nominais para rotulados
- **AND** exibe indicação visual ativa para modificadores de velocidade (`velocity`) e retenção (`hold_during_playback`)

#### Scenario: Filtragem rápida e busca de parâmetros
- **WHEN** o usuário digita um termo na caixa de busca ou ativa o filtro de parâmetros usados no evento ativo
- **THEN** o grid atualiza instantaneamente para exibir apenas os cards correspondentes ao critério informado

#### Scenario: Desacoplamento para janela flutuante e retorno seguro
- **WHEN** o usuário aciona a ação de desencaixe no cabeçalho do deck de Parâmetros Globais
- **THEN** o painel é transferido para uma janela flutuante independente sem botão de fechar do sistema operacional
- **AND** acionar a ação `Dock Back` na janela flutuante reintegra o painel à aba correspondente no Bottom Deck

#### Scenario: Navegação a partir da barra de transporte
- **WHEN** o usuário clica no botão `[Params]` localizado na barra de transporte
- **THEN** o Studio foca e seleciona a aba `Parameters` no Bottom Deck ou traz sua janela flutuante para primeiro plano
