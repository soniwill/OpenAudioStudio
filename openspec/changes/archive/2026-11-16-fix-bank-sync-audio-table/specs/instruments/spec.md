# Spec Delta

## MODIFIED Requirements

### Requirement: Autoria — Compilação e persistência de Programmer Instrument e tabela de áudio
O compilador de projetos (`oae_bankc`) e o formato binário `.bank` MUST suportar instrumentos do tipo `programmer` com `programmer_key` e a tabela de áudio (`audio_table`) mapeando chaves para `sample_id`, preservando compatibilidade com bancos v1 existentes que não declaram tabela de áudio. Toda via de carregamento nativo de banco — incluindo a carga síncrona pela API C — MUST popular a `audio_table` no `ModelSet` reconstruído.

#### Scenario: Compilação e carregamento de Programmer Instrument com audio_table
- **WHEN** um projeto JSON declara uma entrada em `audio_table` associando uma chave a um `sample_id` válido e um evento com instrumento `kind: "programmer"` referenciando essa `programmer_key`
- **THEN** o compilador `oae_bankc` gera o `.bank` sem erro e o carregador nativo popula `programmer_key` e `AudioTableEntry` no `ModelSet`
- **AND** o runtime reproduz o sample associado quando não há callback de usuário registrado

#### Scenario: Carga síncrona pela API C preserva a audio_table
- **WHEN** um banco `.bank` com `audio_table` é carregado pelo caminho síncrono da API C que reconstrói o `ModelSet` a partir do binário
- **THEN** a tabela de áudio do banco MUST estar presente no `ModelSet` reconstruído com todas as entradas chave → `sample_id`
- **AND** um Programmer Instrument sem callback de usuário que referencia uma chave da tabela MUST reproduzir o sample correspondente na prévia

#### Scenario: Rejeição de entrada inválida na audio_table durante compilação
- **WHEN** uma entrada de `audio_table` possui chave vazia, chave duplicada ou referencia um `sample_id` inexistente
- **THEN** o compilador `oae_bankc` falha com erro explícito indicando o caminho da entrada inválida
