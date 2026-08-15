#!/bin/bash

######################################
# TESTE DE DESEMPENHO DO SISTEMA     #
#                                    #
# Este script testa dois aspetos:    #
# 1. Ganho de desempenho ao usar     #
#    múltiplos processos na pesquisa #
#    de documentos (-s "palavra" N)  #
# 2. Impacto do número de documentos #
#    indexados na performance        #
#                                    #
# Para cada cenário (docs e processos),
# mede-se o tempo da operação        #
#                                    #
#  Para rodar o script faz-se:       #
#       ./test_performance.sh        #
#                                    #
# O resultado final é guardado em:   #
#   results/test_results.csv         #
#                                    #
# O CSV contém:                      #
#   N       → nº de documentos       #
#   P       → nº de processos usados #
#   TIME    → tempo da pesquisa (ms) #
######################################

KEYWORD="praia"
DOCSETS=(100 500 1000 1500)
PROCS=(1 2 4 8 16 32 64 128)
GCATALOG="Gcatalog.tsv"
OUTDIR="results"
mkdir -p "$OUTDIR"

SERVER_FOLDER="docs/"
SERVER_EXEC="./bin/dserver"
CLIENT_EXEC="./bin/dclient"
SERVER_PID=""

# Função para iniciar o servidor
start_server() {
    echo "[INFO] Starting server..."
    $SERVER_EXEC "$SERVER_FOLDER" 0 &
    SERVER_PID=$!
    sleep 1
}

# Função para parar o servidor
stop_server() {
    echo "[INFO] Stopping server..."
    $CLIENT_EXEC -f > /dev/null
    wait $SERVER_PID
    sleep 1
}

# Função para limpar documentos anteriores
reset_environment() {
    rm -f persistant_data.bin
}

# Escreve cabeçalho explicativo no CSV
CSV_FILE="$OUTDIR/test_results.csv"
echo "# Teste de desempenho de pesquisa (-s)" > "$CSV_FILE"
echo "# Cada linha: N = nº de documentos, P = nº de processos, TIME = tempo em milisegundos" >> "$CSV_FILE"
echo "N,P,TIME_MS" >> "$CSV_FILE"

# Começar script
echo "[INFO] Running performance tests..."
reset_environment
start_server

for N in "${DOCSETS[@]}"; do
    echo "[INFO] Testing with $N documents"
    
    # Criar subconjunto de Gcatalog
    TMP_FILE="tmp_subset.tsv"
    head -n 1 "$GCATALOG" > "$TMP_FILE"       # header
    head -n $((N+1)) "$GCATALOG" | tail -n $N >> "$TMP_FILE"

    # Indexar documentos
    ./addGdatasetMetadata.sh "$TMP_FILE" > /dev/null

    for P in "${PROCS[@]}"; do
        echo -n "[N=$N, P=$P] "

        # Executar a pesquisa e capturar tempo
        OUTPUT=$($CLIENT_EXEC -s "$KEYWORD" "$P")
        TIME=$(echo "$OUTPUT" | grep "Stopwatch" -A3 | tail -n1 | awk '{print $1}')
        echo "$TIME ms"

        echo "$N,$P,$TIME" >> "$CSV_FILE"
    done

    stop_server
    reset_environment
    start_server
done

stop_server
rm -f tmp_subset.tsv

echo -e "\n[INFO] Done. Results saved in $CSV_FILE"
