# Analisi dei Dominatori: Schema (Notazione IN/OUT)

- **Analisi:**
  - Dominator Analysis

- **Direzione:**
  - Forward

- **Dominio:**
  - Sottoinsiemi dei nodi del CFG (`P(N)`)

- **Operatore di Meet:**
  - Intersezione (`∩`)

- **Equazione IN (Meet Over Paths):**
  - `$IN[n] = \bigcap_{p \in Pred(n)} OUT[p]$`

- **Funzione di Trasferimento (OUT):**
  - `$OUT[n] = IN[n] \cup \{n\}$`

- **Condizione al Contorno:**
  - `OUT[ENTRY] = {ENTRY}`

- **Condizioni Iniziali:**
  - `OUT[n] = N` (insieme di tutti i nodi), per `n ≠ ENTRY`