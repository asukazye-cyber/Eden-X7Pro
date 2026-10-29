#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PLANO 360 TAE 2027 — gerador e gestor do curso.

Uso:
    python3 scripts/plano.py gerar                 # gera os 5 produtos em plano-tae-2027/
    python3 scripts/plano.py validar               # confere o banco de sessoes
    python3 scripts/plano.py hoje                  # briefing "cheguei em casa"
    python3 scripts/plano.py estado                # grava progresso/ESTADO-DO-PLANO.md
    python3 scripts/plano.py marcar S-001 concluida 8/10
    python3 scripts/plano.py marcar S-001 parcial  6/10 "travei em eficacia"
    python3 scripts/plano.py revisao S-001 d1      # registra revisao feita

Os arquivos de dados ficam em plano-tae-2027/dados/sessoes-*.psv, em ordem de estudo.
"""
import csv
import re
import sys
import unicodedata
from collections import defaultdict, OrderedDict
from datetime import date, datetime, timedelta
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent          # plano-tae-2027/
DADOS = RAIZ / "dados"
AULAS = RAIZ / "aulas"
PROG = RAIZ / "progresso"
CSV_PROG = PROG / "progresso.csv"

CAMPOS = ["codigo", "fase", "disc", "assunto", "titulo", "unidades",
          "objetivo", "questoes", "prereq", "criterio"]

DISC_NOME = {
    "ADM": "A1. Administração Geral e Pública",
    "LIC": "A2. Licitações e Contratos (Lei 14.133/2021)",
    "ARQ": "A3. Arquivologia",
    "RED": "A4. Redação Oficial",
    "L811": "B1. Lei nº 8.112/1990",
    "L978": "B2. Lei nº 9.784/1999",
    "CF": "B3. Constituição Federal de 1988",
    "POR": "C1. Língua Portuguesa",
    "RLM": "C2. Raciocínio Lógico e Matemática",
    "INF": "C3. Informática",
    "COM": "D. Conteúdos Complementares (editais TAE)",
    "REV": "E. Revisões e Simulados",
}
MOD = {"ADM": "A", "LIC": "A", "ARQ": "A", "RED": "A",
       "L811": "B", "L978": "B", "CF": "B",
       "POR": "C", "RLM": "C", "INF": "C",
       "COM": "D", "REV": "E"}
MOD_NOME = {
    "A": "MÓDULO A — CONHECIMENTOS ESPECÍFICOS",
    "B": "MÓDULO B — LEGISLAÇÃO FEDERAL",
    "C": "MÓDULO C — CONHECIMENTOS GERAIS",
    "D": "MÓDULO D — COMPLEMENTAR (exigências típicas de edital)",
    "E": "MÓDULO E — REVISÕES E AVALIAÇÕES",
}
CADERNO = {"ADM": "Caderno 2", "LIC": "Caderno 2", "ARQ": "Caderno 2",
           "RED": "Caderno 1*", "L811": "Caderno 1", "L978": "Caderno 1",
           "CF": "Caderno 1", "POR": "Caderno 1", "RLM": "Caderno 1",
           "INF": "Caderno 1", "COM": "Caderno 1", "REV": "Ambos"}
FASE_NOME = {
    "1": "FASE 1 — FUNDAMENTOS (base do zero, todas as disciplinas)",
    "2": "FASE 2 — APROFUNDAMENTO (lei seca, técnicas e detalhes de banca)",
    "3": "FASE 3 — CONSOLIDAÇÃO (conteúdos complementares e fechamento)",
    "4": "FASE 4 — SIMULADOS E RETA FINAL",
}

# ---------------------------------------------------------------- calendário
INICIO = date(2026, 10, 29)              # quinta-feira, primeira noite disponível
CAP_PADRAO = {0: 1, 1: 1, 2: 1, 3: 1, 4: 1, 5: 1, 6: 2}   # seg..dom
# semanas leves (Natal / Ano Novo): terça, sábado e domingo
LEVES = {
    date(2026, 12, 21): {0: 0, 1: 1, 2: 0, 3: 0, 4: 0, 5: 1, 6: 1},
    date(2026, 12, 28): {0: 0, 1: 1, 2: 0, 3: 0, 4: 0, 5: 1, 6: 1},
}
FERIADOS = {date(2027, 2, 15), date(2027, 2, 16), date(2027, 3, 26), date(2027, 5, 27)}
DIAS = ["segunda", "terça", "quarta", "quinta", "sexta", "sábado", "domingo"]


def cap(d):
    if d in FERIADOS:
        return 0
    seg = d - timedelta(days=d.weekday())
    if seg in LEVES:
        return LEVES[seg][d.weekday()]
    return CAP_PADRAO[d.weekday()]


# ---------------------------------------------------------------- utilidades
def data_hoje():
    """Data local do usuário (America/Sao_Paulo), não a do servidor em UTC."""
    try:
        from zoneinfo import ZoneInfo
        return datetime.now(ZoneInfo("America/Sao_Paulo")).date()
    except Exception:
        return (datetime.utcnow() - timedelta(hours=3)).date()


def slug(txt):
    t = unicodedata.normalize("NFKD", txt).encode("ascii", "ignore").decode()
    t = re.sub(r"[^a-zA-Z0-9]+", "-", t).strip("-").lower()
    return t[:68].rstrip("-")


def br(d):
    return d.strftime("%d/%m/%Y")


def arquivo_aula(s):
    return f"aulas/{s['codigo']}-{slug(s['titulo'])}.md"


def unidade_itens(campo):
    """Devolve [(id, nome, bruto)]. Itens sem ID continuam o nome do item anterior."""
    itens = []
    for pedaco in [p.strip() for p in campo.split(";") if p.strip()]:
        mf = re.match(r"^([A-Z][A-Z0-9]{0,6}-\d{3}) a [A-Z][A-Z0-9]{0,6}-\d{3}\b", pedaco)
        m = re.match(r"^([A-Z][A-Z0-9]{0,6}-\d{3})\s+(.*)$", pedaco)
        if mf:
            itens.append((mf.group(1), "faixa", pedaco))
        elif m:
            itens.append((m.group(1), m.group(2), pedaco))
        elif itens:
            uid, nome, bruto = itens[-1]
            itens[-1] = (uid, nome + "; " + pedaco, bruto + "; " + pedaco)
        else:
            itens.append(("—", pedaco, pedaco))
    return itens


# ---------------------------------------------------------------- carga
def carregar():
    sessoes = []
    for arq in sorted(DADOS.glob("sessoes-*.psv")):
        for linha in arq.read_text(encoding="utf-8").splitlines():
            linha = linha.strip()
            if not linha or linha.startswith("#"):
                continue
            partes = [p.strip() for p in linha.split("|")]
            if len(partes) != 10:
                raise SystemExit(f"[ERRO] {arq.name}: {len(partes)} campos -> {linha[:70]}")
            sessoes.append(dict(zip(CAMPOS, partes)))
    return sessoes


def validar(sessoes):
    problemas = []
    vistos = set()
    for s in sessoes:
        if s["codigo"] in vistos:
            problemas.append(f"código repetido: {s['codigo']}")
        vistos.add(s["codigo"])
        if s["disc"] not in DISC_NOME:
            problemas.append(f"{s['codigo']}: disciplina desconhecida {s['disc']}")
        if not s["questoes"].isdigit():
            problemas.append(f"{s['codigo']}: questões inválidas")
        for p in [x.strip() for x in s["prereq"].split(",") if x.strip() and x.strip() != "—"]:
            if p not in vistos and p not in {x["codigo"] for x in sessoes}:
                problemas.append(f"{s['codigo']}: pré-requisito inexistente {p}")
    unidades = defaultdict(list)
    for s in sessoes:
        for uid, nome, _ in unidade_itens(s["unidades"]):
            if uid != "—" and nome != "faixa":
                unidades[uid].append(s["codigo"])
    for uid, usos in unidades.items():
        if len(usos) > 1:
            problemas.append(f"unidade {uid} aparece em {', '.join(usos)}")
    return problemas


# ---------------------------------------------------------------- calendário
def montar_calendario(sessoes):
    """Distribui sessões por dia e agenda revisões com limite diário de carga."""
    dia_sessoes = OrderedDict()
    i = 0
    d = INICIO
    while i < len(sessoes):
        c = cap(d)
        if c:
            dia_sessoes[d] = []
            for _ in range(c):
                if i < len(sessoes):
                    dia_sessoes[d].append(sessoes[i]["codigo"])
                    i += 1
        d += timedelta(days=1)
    fim_nucleo = max(dia_sessoes)
    dias = sorted(dia_sessoes)

    def proximo_dia_estudo(x):
        for y in dias:
            if y >= x:
                return y
        y = x
        while y.weekday() == 6:      # domingo fora do plano: usa segunda
            y += timedelta(days=1)
        return y

    # carga de revisão: D+1 = 3 questões, D+7 e D+30 = 5 questões; máx. 3 blocos por dia
    LIMITE = 4
    revisoes = defaultdict(list)
    for d in dias:
        for cod in dia_sessoes[d]:
            for off, tag, nq, flex in ((1, "D+1", 3, 0), (7, "D+7", 5, 3), (30, "D+30", 5, 6)):
                alvo = proximo_dia_estudo(d + timedelta(days=off))
                tentativas = 0
                while flex and len(revisoes[alvo]) >= LIMITE and tentativas < flex:
                    alvo = proximo_dia_estudo(alvo + timedelta(days=1))
                    tentativas += 1
                revisoes[alvo].append((cod, tag, nq))

    # dias finais que só têm revisão (até 40 dias após o núcleo)
    for d in sorted(revisoes):
        if d > fim_nucleo and d <= fim_nucleo + timedelta(days=40):
            dia_sessoes.setdefault(d, [])
    dia_sessoes = OrderedDict(sorted(dia_sessoes.items()))
    return dia_sessoes, revisoes, fim_nucleo


# ---------------------------------------------------------------- saídas
def gerar_matriz(sessoes):
    indice = {s["codigo"]: i for i, s in enumerate(sessoes)}
    linhas = ["# PRODUTO 1 — MATRIZ DE CONTEÚDO", "",
              f"*Plano de Estudos 360 TAE 2027 · gerado em {br(data_hoje())} · "
              f"{len(sessoes)} sessões · banco de dados: `dados/sessoes-*.psv`*", "",
              "Cada unidade de estudo tem identificador único (ADM-001, LIC-007, ...). "
              "A coluna **Sessão** indica onde a unidade é estudada; a coluna **Pré-requisito** "
              "indica o que precisa estar dominado antes. Revisões: toda sessão entra "
              "automaticamente em D+1, D+7 e D+30 (ver `04-sistema-de-revisoes.md`).", ""]

    # resumo
    linhas += ["## 0. Visão geral por disciplina", "",
               "| Módulo | Disciplina | Unidades | Sessões | Questões | Caderno |",
               "|---|---|---:|---:|---:|---|"]
    for disc, nome in DISC_NOME.items():
        ss = [s for s in sessoes if s["disc"] == disc]
        un = len({u for s in ss for u, n, _ in unidade_itens(s["unidades"]) if u != "—" and n != "faixa"})
        q = sum(int(s["questoes"]) for s in ss)
        linhas.append(f"| {MOD[disc]} | {nome} | {un} | {len(ss)} | {q} | {CADERNO[disc]} |")
    tot_un = len({u for s in sessoes for u, n, _ in unidade_itens(s["unidades"]) if u != "—" and n != "faixa"})
    tot_q = sum(int(s["questoes"]) for s in sessoes)
    linhas += [f"| — | **TOTAL** | **{tot_un}** | **{len(sessoes)}** | **{tot_q}** | — |", "",
               "\\* Redação Oficial: teoria e padrão no Caderno 1; prática de expedientes e erros no Caderno 2.",
               ""]

    # árvore
    for m in ["A", "B", "C", "D", "E"]:
        disc_m = [d for d in DISC_NOME if MOD[d] == m]
        linhas += [f"## {MOD_NOME[m]}", ""]
        for disc in disc_m:
            ss = [s for s in sessoes if s["disc"] == disc]
            if not ss:
                continue
            linhas += [f"### {DISC_NOME[disc]}", "",
                       f"*{len(ss)} sessões · {sum(int(x['questoes']) for x in ss)} questões · "
                       f"{CADERNO[disc]}*", ""]
            por_assunto = OrderedDict()
            for s in ss:
                por_assunto.setdefault(s["assunto"], []).append(s)
            for assunto, lista in por_assunto.items():
                linhas += [f"**Assunto: {assunto}**", "",
                           "| Unidade | Conteúdo a dominar | Sessão | Pré-requisito |",
                           "|---|---|---|---|"]
                for s in lista:
                    for uid, nome, bruto in unidade_itens(s["unidades"]):
                        if nome == "faixa":
                            linhas.append(f"| {uid}… | Revisão integrada do bloco | {s['codigo']} | {s['prereq']} |")
                        else:
                            linhas.append(f"| {uid} | {nome} | {s['codigo']} | {s['prereq']} |")
                linhas.append("")
    (RAIZ / "01-matriz-de-conteudo.md").write_text("\n".join(linhas), encoding="utf-8")
    return tot_un, tot_q


def gerar_cronograma(sessoes, dia_sessoes):
    data_de = {}
    for d, cods in dia_sessoes.items():
        for c in cods:
            data_de[c] = d
    linhas = ["# PRODUTO 2 — CRONOGRAMA DE SESSÕES", "",
              f"*{len(sessoes)} sessões · gerado em {br(data_hoje())}*", "",
              "## Como ler cada sessão", "",
              "Toda sessão tem os dez itens pedidos. Para não repetir texto 200 vezes, "
              "os itens 5, 6, 7 e 9 seguem a **política fixa** abaixo e cada aula detalha o resto:", "",
              "| Item | Onde está |", "|---|---|",
              "| 1. Código | coluna **Cód.** |",
              "| 2. Disciplina e assunto | cabeçalho de cada tabela |",
              "| 3. Objetivo de aprendizagem | coluna **Objetivo** |",
              "| 4. Conteúdo específico | coluna **Unidades** |",
              "| 5. Material teórico | `aulas/<Cód>-<slug>.md` (Nível A) — ver Produto 5 |",
              "| 6. Videoaula | termos de busca por disciplina em `06-biblioteca-de-fontes.md`; "
              "aula específica quando verificada |",
              "| 7. O que anotar | seção **ANOTE NO CADERNO** da aula (Nível B), no caderno indicado |",
              "| 8. Quantidade de questões | coluna **Q** |",
              "| 9. Revisões | D+1, D+7 e D+30 automáticas, na data real do calendário (Produto 3) |",
              "| 10. Critério de conclusão | coluna **Conclusão** |", "",
              "**Regra de conclusão:** a sessão só é concluída quando o critério da última coluna "
              "é cumprido *e* as questões são corrigidas com registro dos erros. Assistir aula não conclui sessão.", ""]
    for fase in ["1", "2", "3", "4"]:
        ss = [s for s in sessoes if s["fase"] == fase]
        if not ss:
            continue
        linhas += [f"## {FASE_NOME[fase]}", "",
                   f"*{len(ss)} sessões · {sum(int(s['questoes']) for s in ss)} questões · "
                   f"de {br(data_de[ss[0]['codigo']])} a {br(data_de[ss[-1]['codigo']])}*", ""]
        for chave in OrderedDict.fromkeys((s["disc"], s["assunto"]) for s in ss):
            disc, assunto = chave
            grupo = [s for s in ss if (s["disc"], s["assunto"]) == chave]
            linhas += [f"### {DISC_NOME[disc]} — {assunto}", "",
                       "| Cód. | Sessão e objetivo | Unidades | Q | Pré | Conclusão |",
                       "|---|---|---|---:|---|---|"]
            for s in grupo:
                unidades = "; ".join(f"**{u}**" for u, n, _ in unidade_itens(s["unidades"]) if n != "faixa") \
                    or "revisão integrada"
                linhas.append(
                    f"| **{s['codigo']}** | {s['titulo']}<br>*{s['objetivo']}* | {unidades} | "
                    f"{s['questoes']} | {s['prereq']} | {s['criterio']} |")
            linhas.append("")
    (RAIZ / "02-cronograma-de-sessoes.md").write_text("\n".join(linhas), encoding="utf-8")


def gerar_calendario(sessoes, dia_sessoes, revisoes, fim):
    qtd = {s["codigo"]: int(s["questoes"]) for s in sessoes}
    titulo = {s["codigo"]: s["titulo"] for s in sessoes}
    disc = {s["codigo"]: s["disc"] for s in sessoes}

    linhas = ["# PRODUTO 3 — CALENDÁRIO EXECUTÁVEL", "",
              f"*Início: {br(INICIO)} (quinta) · previsão de término do núcleo: **{br(fim)}** · "
              f"gerado em {br(data_hoje())}*", "",
              "## 1. Capacidade semanal adotada", "",
              "| Dia | Sessões novas | Janela sugerida |", "|---|---:|---|",
              "| Segunda a sexta | 1 | 20h00–21h00 (chegando em casa) |",
              "| Sábado | 1 | manhã ou tarde, horário livre |",
              "| Domingo | 2 | 09h00 e 14h00, com pausa de 30 min |",
              "| Domingo (3º bloco) | 0 | **revisão semanal + colchão de atraso** |", "",
              "Total: **8 sessões novas por semana** (≈ 8 h de teoria) + 15 min de revisão rápida "
              "por dia + 1 bloco de colchão no domingo. Nas semanas de Natal e Ano Novo a carga cai "
              "para 3 sessões, de propósito.", "",
              "## 2. Regras de execução", "",
              "1. **Ordem rígida:** faça as sessões na ordem do calendário. Pré-requisito existe por um motivo.",
              "2. **Meta mínima:** em dia cansado, faça só a coluna *Meta mínima* (25–30 min). "
              "Ela não é o ideal, é o mínimo que mantém a memória viva.",
              "3. **Atraso:** o 3º bloco de domingo é o colchão. Se acumular mais de 4 sessões, "
              "use sábado + domingo e **não** pule disciplinas: corte questões, não conteúdo.",
              "4. **Revisões têm prioridade** sobre sessão nova. Revisão atrasada = 2 dias no máximo.",
              "5. **Feriado ou imprevisto:** a sessão do dia desliza para o próximo slot; nada é apagado.",
              "6. **Dia perdido por completo:** registre `marcar <cód> parcial` ou ignore; o colchão absorve.",
              "7. **Edital publicado:** pare a sequência e passe para o modo edital (seção 4 de `00-LEIA-PRIMEIRO.md`).", ""]

    # resumo mensal
    por_mes = OrderedDict()
    for d in dia_sessoes:
        por_mes.setdefault((d.year, d.month), []).append(d)
    linhas += ["## 3. Resumo mensal", "",
               "| Mês | Sessões | Questões | Revisões programadas |", "|---|---:|---:|---:|"]
    for (a, m), dias in por_mes.items():
        ns = sum(len(dia_sessoes[d]) for d in dias)
        nq = sum(qtd[c] for d in dias for c in dia_sessoes[d]) + \
             sum(r[2] for d in dias for r in revisoes.get(d, []))
        nr = sum(len(revisoes.get(d, [])) for d in dias)
        linhas.append(f"| {m:02d}/{a} | {ns} | {nq} | {nr} |")
    linhas.append("")

    # mês a mês
    linhas += ["## 4. Calendário dia a dia", "",
               "Legenda: cada linha é uma noite. **Meta mínima** é o que fazer quando o dia foi ruim "
               "(25–30 min). Questões = sessões do dia + 5 por revisão.", ""]
    for (a, m), dias in por_mes.items():
        linhas += [f"### {m:02d}/{a}", "",
                   "| Data | Dia | Sessão(ões) | Revisões do dia | Questões | Meta mínima |",
                   "|---|---|---|---|---:|---|"]
        d0 = dias[0]
        while d0 <= dias[-1]:
            if d0 in dia_sessoes:
                cods = dia_sessoes[d0]
                revs = revisoes.get(d0, [])
                rev_txt = ", ".join(f"{c} ({t})" for c, t, _ in revs) if revs else "—"
                nq = sum(qtd[c] for c in cods) + sum(r[2] for r in revs)
                if cods:
                    sess_txt = "<br>".join(f"**{c}** · {disc[c]} · {titulo[c]}" for c in cods)
                    if d0.weekday() == 6:
                        sess_txt += "<br>🔁 revisão semanal + colchão de atraso"
                    meta = f"25–30 min: {cods[0]} — ler o Nível B e fazer 5 questões"
                else:
                    sess_txt = "— (só revisões)"
                    meta = "20 min: flashcards + caderno de erros"
            else:
                d0 += timedelta(days=1)
                continue
            linhas.append(f"| {br(d0)} | {DIAS[d0.weekday()]} | {sess_txt} | {rev_txt} | {nq} | {meta} |")
            d0 += timedelta(days=1)
        linhas.append("")
    (RAIZ / "03-calendario-executavel.md").write_text("\n".join(linhas), encoding="utf-8")


def gerar_indice(sessoes):
    linhas = ["# PRODUTO 5 — ÍNDICE DO MATERIAL DIDÁTICO", "",
              f"*gerado em {br(data_hoje())}*", "",
              "## Nomenclatura", "",
              "```",
              "aulas/S-001-conceitos-de-administracao-organizacoes-e-eficiencia.md",
              "```",
              "Cada arquivo de aula contém os **três níveis** de material, em seções fixas:", "",
              "| Nível | Seção interna | Para que serve |", "|---|---|---|",
              "| **A** | `## PARTE 1 — MATERIAL BRUTO COMPLETO` | estudar a teoria do zero |",
              "| **B** | `## ANOTE NO CADERNO` | copiar à mão no caderno físico |",
              "| **C** | `## PARTE 2 — REVISÃO ATIVA` | recordar, treinar e caçar erros |", "",
              "Assim você localiza o dia pelo código da sessão (`Ctrl+F` por `S-0xx`) e não perde "
              "material quando o arquivo é renomeado no futuro. Os códigos de sessão e de unidade "
              "nunca mudam.", "",
              "## Status de produção", "",
              "| Cód. | Arquivo de material | Caderno | Status |", "|---|---|---|---|"]
    prontos = 0
    for s in sessoes:
        arq = arquivo_aula(s)
        existe = (RAIZ / arq).exists()
        prontos += existe
        linhas.append(f"| {s['codigo']} | `{arq}` | {CADERNO[s['disc']]} | "
                      f"{'✅ pronto' if existe else '⏳ a produzir'} |")
    linhas += ["", f"**Produzidos: {prontos} de {len(sessoes)}.** "
                   "As aulas são liberadas em blocos, na ordem do calendário; "
                   "peça com `GERAR MATERIAL DA PRÓXIMA SESSÃO` ou `GERAR BLOCO DE 5 SESSÕES`.",
              ""]
    (RAIZ / "05-indice-de-materiais.md").write_text("\n".join(linhas), encoding="utf-8")


def gerar_tudo():
    sessoes = carregar()
    problemas = validar(sessoes)
    if problemas:
        print("[AVISO] problemas no banco de sessões:")
        for p in problemas:
            print("  -", p)
    dia_sessoes, revisoes, fim = montar_calendario(sessoes)
    u, q = gerar_matriz(sessoes)
    gerar_cronograma(sessoes, dia_sessoes)
    gerar_calendario(sessoes, dia_sessoes, revisoes, fim)
    gerar_indice(sessoes)
    print(f"OK: {len(sessoes)} sessões, {u} unidades, {q} questões, "
          f"início {br(INICIO)}, fim {br(fim)}")
    print("Gerados: 01-matriz-de-conteudo.md, 02-cronograma-de-sessoes.md, "
          "03-calendario-executavel.md, 05-indice-de-materiais.md")


# ---------------------------------------------------------------- progresso
def carregar_progresso(sessoes):
    PROG.mkdir(exist_ok=True)
    if not CSV_PROG.exists():
        with CSV_PROG.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f, delimiter="|")
            w.writerow(["codigo", "status", "data_conclusao", "acertos", "total",
                        "rev_d1", "rev_d7", "rev_d30", "obs"])
            for s in sessoes:
                w.writerow([s["codigo"], "pendente", "", "", s["questoes"], "", "", "", ""])
    with CSV_PROG.open(encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="|"))


def salvar_progresso(regs):
    with CSV_PROG.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["codigo", "status", "data_conclusao", "acertos",
                                          "total", "rev_d1", "rev_d7", "rev_d30", "obs"],
                           delimiter="|")
        w.writeheader()
        w.writerows(regs)


def marcar(cod, status, acertos=None, obs=""):
    sessoes = carregar()
    regs = carregar_progresso(sessoes)
    achou = False
    for r in regs:
        if r["codigo"] == cod:
            achou = True
            r["status"] = status
            if status == "concluida":
                r["data_conclusao"] = data_hoje().isoformat()
            if acertos:
                a, _, t = acertos.partition("/")
                r["acertos"], r["total"] = a, t or r["total"]
            if obs:
                r["obs"] = obs
    if not achou:
        raise SystemExit(f"sessão {cod} não existe")
    salvar_progresso(regs)
    print(f"{cod} -> {status}")


def revisao(cod, tipo):
    sessoes = carregar()
    regs = carregar_progresso(sessoes)
    campo = {"d1": "rev_d1", "d7": "rev_d7", "d30": "rev_d30"}[tipo]
    for r in regs:
        if r["codigo"] == cod:
            r[campo] = data_hoje().isoformat()
            salvar_progresso(regs)
            print(f"{cod}: revisão {tipo} registrada")
            return
    raise SystemExit(f"sessão {cod} não existe")


def estado(gravar=True):
    sessoes = carregar()
    regs = carregar_progresso(sessoes)
    dia_sessoes, revisoes, fim = montar_calendario(sessoes)
    status = {r["codigo"]: r for r in regs}
    concl = [c for c, r in status.items() if r["status"] == "concluida"]
    parc = [c for c, r in status.items() if r["status"] == "parcial"]
    prox = next((s for s in sessoes if status[s["codigo"]]["status"] != "concluida"), None)

    hoje = data_hoje()
    pend = []
    for r in regs:
        if r["status"] != "concluida":
            continue
        try:
            dc = datetime.strptime(r["data_conclusao"], "%Y-%m-%d").date()
        except ValueError:
            continue
        for tag, off, campo in (("D+1", 1, "rev_d1"), ("D+7", 7, "rev_d7"), ("D+30", 30, "rev_d30")):
            if not r[campo] and hoje >= dc + timedelta(days=off):
                pend.append((r["codigo"], tag, (hoje - (dc + timedelta(days=off))).days))

    por_disc = defaultdict(lambda: [0, 0])
    for r in regs:
        d = next((s["disc"] for s in sessoes if s["codigo"] == r["codigo"]), "?")
        if r["acertos"]:
            por_disc[d][0] += int(r["acertos"])
            por_disc[d][1] += int(r["total"])

    txt = [f"# ESTADO DO PLANO — {br(hoje)}", "",
           f"- Sessões planejadas: **{len(sessoes)}**",
           f"- Concluídas: **{len(concl)}** | parciais: **{len(parc)}** | pendentes: "
           f"**{len(sessoes) - len(concl)}**",
           f"- Progresso: **{100 * len(concl) / len(sessoes):.1f}%**",
           f"- Previsão de término do núcleo: **{br(fim)}**",
           f"- Próxima sessão: **{prox['codigo']} — {prox['titulo']}** ({prox['disc']}, {prox['questoes']} questões)",
           ""]
    if por_disc:
        txt += ["## Acertos por disciplina (só sessões com nota registrada)", "",
                "| Disciplina | Acertos | % |", "|---|---:|---:|"]
        for d, (a, t) in sorted(por_disc.items()):
            txt.append(f"| {DISC_NOME.get(d, d)} | {a}/{t} | {100 * a / t:.0f}% |")
        txt.append("")
    txt += ["## Revisões pendentes", ""]
    if pend:
        txt += [f"- {c} — {t} (atrasada {dias} dia(s))" for c, t, dias in sorted(pend, key=lambda x: -x[2])]
    else:
        txt.append("- nenhuma")
    txt += ["", "## Como retomar em outro chat", "",
            "Cole este bloco e diga **CONTINUAR PLANO**. O estado real está em "
            "`progresso/progresso.csv`; este resumo é a fotografia do dia.", ""]
    texto = "\n".join(txt)
    if gravar:
        PROG.mkdir(exist_ok=True)
        (PROG / "ESTADO-DO-PLANO.md").write_text(texto, encoding="utf-8")
    print(texto)


def hoje():
    sessoes = carregar()
    regs = carregar_progresso(sessoes)
    dia_sessoes, revisoes, _ = montar_calendario(sessoes)
    status = {r["codigo"]: r for r in regs}
    pend = [s for s in sessoes if status[s["codigo"]]["status"] != "concluida"]
    if not pend:
        print("Curso concluído. Hora dos simulados e da revisão espaçada.")
        return
    prox = pend[0]
    data_prev = next((d for d, cods in dia_sessoes.items() if prox["codigo"] in cods), None)
    hoje_d = data_hoje()
    rev_hoje = revisoes.get(hoje_d, [])
    print("=" * 62)
    print(f"BRIEFING DO DIA — {br(hoje_d)} ({DIAS[hoje_d.weekday()]})")
    print("=" * 62)
    if data_prev:
        atraso = (hoje_d - data_prev).days
        sit = "em dia" if atraso <= 0 else f"{atraso} dia(s) após a data prevista"
        print(f"Calendário: {br(data_prev)} ({sit})")
    print(f"SESSÃO: {prox['codigo']} — {prox['titulo']}")
    print(f"Disciplina: {DISC_NOME[prox['disc']]} | Caderno: {CADERNO[prox['disc']]}")
    print(f"Tempo: ~60 min | Questões: {prox['questoes']}")
    print(f"Objetivo: {prox['objetivo']}")
    print(f"Material: {arquivo_aula(prox)}")
    print(f"Conclusão: {prox['criterio']}")
    if rev_hoje:
        print("\nREVISÕES DE HOJE:")
        for c, t in rev_hoje:
            print(f"  - {c} ({t}) — recordação ativa + 5 questões")
    else:
        print("\nREVISÕES DE HOJE: nenhuma")
    print("\nROTEIRO (60 min):")
    print("  5 min  recordação ativa do que estudou ontem (sem consultar)")
    print("  30 min teoria do Nível A + ANOTE NO CADERNO")
    print("  20 min questões (corrigir e registrar erros no Caderno 2)")
    print("  5 min revisão ativa: perguntas de recordação do fim da aula")
    print("\nRegistrar depois: python3 scripts/plano.py marcar "
          f"{prox['codigo']} concluida 8/{prox['questoes']}")


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "gerar"
    if cmd == "gerar":
        gerar_tudo()
    elif cmd == "validar":
        sessoes = carregar()
        probs = validar(sessoes)
        print(f"{len(sessoes)} sessões carregadas.")
        if probs:
            print("\n".join(probs))
        else:
            print("nenhum problema encontrado")
    elif cmd == "hoje":
        hoje()
    elif cmd == "estado":
        estado()
    elif cmd == "marcar":
        marcar(sys.argv[2], sys.argv[3],
               sys.argv[4] if len(sys.argv) > 4 else None,
               sys.argv[5] if len(sys.argv) > 5 else "")
    elif cmd == "revisao":
        revisao(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
