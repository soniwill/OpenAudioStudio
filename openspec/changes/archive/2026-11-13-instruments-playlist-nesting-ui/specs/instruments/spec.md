# Spec Delta

## ADDED Requirements

### Requirement: Autoria — Compilação e persistência de Programmer Instrument e tabela de áudio
O compilador de projetos (`oae_bankc`) e o formato binário `.bank` MUST suportar instrumentos do tipo `programmer` com `programmer_key` e a tabela de áudio (`audio_table`) mapeando chaves para `sample_id`, preservando compatibilidade com bancos v1 existentes que não declaram tabela de áudio.

#### Scenario: Compilação e carregamento de Programmer Instrument com audio_table
- **WHEN** um projeto JSON declara uma entrada em `audio_table` associando uma chave a um `sample_id` válido e um evento com instrumento `kind: "programmer"` referenciando essa `programmer_key`
- **THEN** o compilador `oae_bankc` gera o `.bank` sem erro e o carregador nativo popula `programmer_key` e `AudioTableEntry` no `ModelSet`
- **AND** o runtime reproduz o sample associado quando não há callback de usuário registrado

#### Scenario: Rejeição de entrada inválida na audio_table durante compilação
- **WHEN** uma entrada de `audio_table` possui chave vazia, chave duplicada ou referencia um `sample_id` inexistente
- **THEN** o compilador `oae_bankc` falha com erro explícito indicando o caminho da entrada inválida

### Requirement: Autoria — Entradas target_event em playlists de Multi e Scatterer
O `AuthoringModel` e a validação de projeto MUST suportar entradas de playlist com `target_event` (além de `sample_id`) em instrumentos `multi` e `scatterer`, rejeitando referências que criem ciclos diretos ou indiretos entre eventos.

#### Scenario: Validação anti-ciclo em playlist mista
- **WHEN** um usuário ou projeto tenta adicionar `target_event` na playlist de um `Multi` ou `Scatterer`
- **THEN** apenas eventos que não formam ciclo direto ou indireto com o evento pai são aceitos como alvos válidos
