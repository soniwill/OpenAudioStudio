# Spec Delta

## ADDED Requirements

### Requirement: Runtime — Sound Stealing Oldest na polifonia do Scatterer Instrument
Quando um `Scatterer Instrument` precisar disparar um novo grão e a contagem de grãos ativos simultaneamente atingir `polyphony` (`polyphony > 0`), o motor de áudio MUST aplicar *Sound Stealing* (`Oldest`), interrompendo o grão ativo mais antigo daquele spawner para liberar uma vaga antes do novo disparo.

#### Scenario: Roubo do grão mais antigo quando polifonia máxima é atingida
- **WHEN** um `Scatterer Instrument` com `polyphony = 2` já mantém 2 grãos ativos e o temporizador solicita o disparo de um terceiro grão
- **THEN** o grão mais antigo em execução daquele spawner é interrompido imediatamente
- **AND** o terceiro grão é disparado preservando o teto simultâneo de 2 grãos ativos
