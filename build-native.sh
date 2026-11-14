#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
for arg in "$@"; do
    case "$arg" in
        --public) echo 'La beta usa solo cookie locali; non servono credenziali OAuth.' ;;
        *) echo "Argomento non riconosciuto: $arg" >&2; exit 2 ;;
    esac
done
command -v docker >/dev/null || { echo 'Serve Docker. Installa Docker e riprova.' >&2; exit 1; }
DOCKER=(docker)
if ! docker info >/dev/null 2>&1; then
    DOCKER=(sudo docker)
fi
"${DOCKER[@]}" info >/dev/null
python3 scripts/check-sdk.py
python3 scripts/check-translations.py
mkdir -p output/logs
rm -f output/KaiTube-beta0.0.2.nro
set +e
(
    set -e
    "${DOCKER[@]}" build -f scripts/Dockerfile.native -t kaitube-native-sdk:20261114 .
    "${DOCKER[@]}" run --rm -v "$PWD:/work" -w /work kaitube-native-sdk:20261114 bash scripts/build-app.sh
) 2>&1 | tee output/logs/build-native.log
result=${PIPESTATUS[0]}
set -e
"${DOCKER[@]}" run --rm -v "$PWD:/work" kaitube-native-sdk:20261114 chown -R "$(id -u):$(id -g)" /work/output /work/cmake-build-switch2 /work/.build-native-generator 2>/dev/null || true
if [[ "$result" != 0 ]]; then
    echo 'Build interrotta. Invia output/logs/build-native.log.' >&2
    exit "$result"
fi
echo 'Creato: output/KaiTube-beta0.0.2.nro'
