# COMO REGISTRAR O PROGRESSO

## 1. O que conta como concluída

| Status | Quando usar |
|---|---|
| `pendente` | não começou |
| `parcial` | leu, mas não fez as questões ou não cumpriu o critério |
| `concluida` | leu + copiou o Nível B + fez e corrigiu as questões + cumpriu o critério da sessão |

Nunca marque `concluida` só porque a aula foi apresentada.

## 2. No computador (rápido)

```bash
python3 scripts/plano.py marcar S-001 concluida 8/10
python3 scripts/plano.py marcar S-002 parcial 5/10 "travei em tipologia textual"
python3 scripts/plano.py revisao S-001 d1
python3 scripts/plano.py hoje
python3 scripts/plano.py estado
```

## 3. No papel (se preferir)

Na primeira página de cada caderno, desenhe uma tabela:

| Sessão | Data | Status | Acertos | Rev. D+1 | Rev. D+7 | Rev. D+30 |
|---|---|---|---|---|---|---|
| S-001 | 29/10 | concluída | 8/10 | ☑ | ☐ | ☐ |

## 4. O bloco ESTADO DO PLANO

Ao final de cada sessão, eu gero um bloco assim (e ele também vai para `progresso/ESTADO-DO-PLANO.md`):

```
ESTADO DO PLANO — 29/10/2026
Sessões: 1/200 concluídas (0,5%) | parciais: 0
Última concluída: S-001 (8/10)
Próxima: S-002 — Leitura, interpretação e inferência (POR, 10 questões)
Revisões pendentes: nenhuma
Fragilidades registradas: nenhuma
Como retomar: cole este bloco e diga CONTINUAR PLANO
```

Isso resolve o problema de trocar de chat: o estado está escrito, não na minha memória.

## 5. Percentual de acertos por disciplina

Só registre a nota quando você **corrigiu** as questões. A conta que importa:
`acertos da disciplina ÷ total de questões da disciplina`
Abaixo de 60% = disciplina frágil (volta para a fila de revisão). Entre 60% e 79% = em construção. 80%+ = consolidando.
