# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Governança visual unificada e tokens de design no Studio
O OpenAudio Studio SHALL aderir às diretrizes de interface especificadas em `docs/UI_GUIDELINES.md`, aplicando cores semânticas, escala tipográfica e espaçamentos múltiplos de 4px de forma consistente em todos os painéis e diálogos através do tema centralizado.

#### Scenario: Consistência visual e temática nos painéis
- **WHEN** qualquer painel do Studio (Navegador, Timeline, Inspetor, Mixer ou Decks) for renderizado na tela
- **THEN** suas cores de superfície, bordas, textos e acentos funcionais MUST derivar exclusivamente dos tokens de tema definidos no Design System.

### Requirement: Controles rotatórios vetoriais com arco proporcional e indicador central
Os controles do tipo rotary slider (knobs) no OpenAudio Studio SHALL ser renderizados inteiramente via primitivas vetoriais com arco proporcional ao valor e indicador central nítido, sem depender de imagens bitmap estáticas.

#### Scenario: Renderização vetorial responsiva
- **WHEN** um knob de parâmetro, ganho ou efeito for exibido ou redimensionado
- **THEN** o Studio MUST desenhar o fundo do trilho, o arco de valor preenchido e o ponteiro central usando geometria vetorial nítida em qualquer fator de escala da janela.

### Requirement: Ajuste fino com Shift e reset por duplo-clique em controles numéricos
Os sliders e knobs do OpenAudio Studio SHALL reduzir a sensibilidade de arraste do mouse para ajuste fino quando a tecla `Shift` for mantida pressionada e SHALL restaurar o valor padrão (`default_value`) do parâmetro ao receber um duplo-clique.

#### Scenario: Ajuste fino com tecla Shift pressionada
- **GIVEN** um controle numérico (knob ou slider) sendo arrastado pelo usuário com o mouse
- **WHEN** a tecla `Shift` estiver pressionada durante o arraste
- **THEN** a sensibilidade de deslocamento MUST ser reduzida em relação ao deslocamento padrão para permitir ajuste fino com precisão de décimos de unidade.

#### Scenario: Restauração de valor padrão por duplo clique
- **WHEN** o usuário der um duplo-clique sobre um knob ou slider que possua valor padrão definido
- **THEN** o controle MUST retornar imediatamente ao seu `default_value` e registrar a alteração no histórico de Undo/Redo.

### Requirement: Medidores de nível dBFS vetoriais com decaimento balístico
Os medidores de nível de áudio e de pico (VU meters) do Studio SHALL ser desenhados vetorialmente e atualizados a uma taxa de 30 a 60 FPS com decaimento balístico suave e indicador de retenção de pico (peak hold).

#### Scenario: Exibição balística durante reprodução
- **GIVEN** um evento de áudio em reprodução ativa emitindo sinal para um bus ou fader de pista
- **WHEN** o sinal de áudio oscilar em nível sonoro
- **THEN** o medidor dBFS MUST atualizar suas barras vetoriais com animação de subida instantânea e queda balística amortecida, mantendo o indicador de pico visível temporariamente.

### Requirement: Layout fluido sem coordenadas fixas nos painéis internos
Os painéis internos do OpenAudio Studio SHALL organizar seus elementos filhos utilizando exclusivamente modelos de layout flexíveis e responsivos (`FlexBox` ou `Grid`), redistribuindo os controles fluidamente ao redimensionar a janela ou os divisores sem truncar componentes.

#### Scenario: Redimensionamento fluido de painéis e barras
- **WHEN** a janela principal ou um divisor de área (`SplitterBar`) for movido pelo usuário
- **THEN** os painéis afetados MUST readequar o espaçamento, quebras de linha e dimensões de seus botões e campos através de FlexBox/Grid sem sobreposição de controles ou limites manuais fixos soltos.

### Requirement: Reorganização de painéis por abas e janelas destacáveis (Pop-out)
O sistema de layout do OpenAudio Studio SHALL permitir reorganizar painéis em abas nos slots de encaixe e permitir destacar painéis pesados (como Mixer) em janelas flutuantes independentes, com capacidade de retorno ao dock.

#### Scenario: Destacar painel em janela flutuante
- **WHEN** o usuário acionar o comando ou botão de destaque (*pop-out*) em um painel do Studio
- **THEN** o painel MUST ser removido do slot ancorado e aberto em uma janela flutuante redimensionável e operável simultaneamente ao restante da aplicação.

#### Scenario: Retorno de janela destacada ao dock principal
- **WHEN** a janela flutuante destacada for fechada ou o comando de retorno for acionado
- **THEN** o painel MUST retornar à sua posição e slot de ancoragem original na casca do Studio.
