# Spec Delta

## ADDED Requirements

### Requirement: Posicionamento da audição local pela régua com o transporte parado

O Studio SHALL guardar a posição escolhida na régua de tempo do evento atual enquanto a audição local está parada, refletir essa posição nos indicadores de tempo e usá-la como ponto inicial da próxima audição local do evento. O Studio SHALL limpar a posição ao acionar Stop ou ao trocar o evento de audição.

#### Scenario: Escolha de posição com a audição parada

- **WHEN** o usuário clica ou arrasta em uma área da régua de tempo livre de itens lógicos com a audição local parada
- **THEN** o ponteiro da régua, o minimapa e os contadores mostram a posição resultante do gesto, com as regras de snap da régua
- **AND** os indicadores permanecem nessa posição nas atualizações seguintes, com a audição ainda parada

#### Scenario: Início da audição na posição escolhida

- **WHEN** o usuário pressiona Play local após selecionar uma posição válida diferente de zero na régua com o transporte parado
- **THEN** a audição local começa nessa posição e o ponteiro acompanha o avanço da reprodução

#### Scenario: Limpeza pelo comando Stop

- **WHEN** o usuário aciona Stop depois de escolher uma posição com a audição parada ou durante a audição local iniciada desse ponto
- **THEN** o ponteiro, o minimapa e os contadores retornam a zero e o próximo Play local começa do início, salvo nova escolha na régua

#### Scenario: Troca do evento de audição

- **WHEN** o usuário muda o evento de audição depois de escolher uma posição com o transporte parado
- **THEN** os indicadores do evento recém-selecionado mostram zero e seu primeiro Play local começa do início, salvo nova escolha na régua
