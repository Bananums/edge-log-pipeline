#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# setup.sh — Prepares the host for running the observability stack.
# Run this once before `docker compose up` on a fresh machine or VPS.
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/.env"
ENV_EXAMPLE="${SCRIPT_DIR}/.env.example"

# --- Ensure .env exists -----------------------------------------------------

if [[ ! -f "${ENV_FILE}" ]]; then
  echo "[setup] No .env found, copying from .env.example"
  cp "${ENV_EXAMPLE}" "${ENV_FILE}"
  echo "[setup] Please edit ${ENV_FILE} and re-run this script."
  exit 1
fi

# --- Load .env --------------------------------------------------------------

# shellcheck source=/dev/null
source "${ENV_FILE}"

# --- Validate required variables --------------------------------------------

required_vars=(
  LOKI_DATA_DIR
  GRAFANA_DATA_DIR
  GRAFANA_ADMIN_USER
  GRAFANA_ADMIN_PASSWORD
)

missing=0
for var in "${required_vars[@]}"; do
  if [[ -z "${!var:-}" ]]; then
    echo "[setup] ERROR: ${var} is not set in .env"
    missing=1
  fi
done

if [[ "${missing}" -eq 1 ]]; then
  exit 1
fi

# --- Loki -------------------------------------------------------------------

# Loki runs as uid 10001 inside the container.
# The data directory must be owned by that uid or Loki will fail to write.
LOKI_UID=10001

echo "[setup] Creating Loki data directory: ${LOKI_DATA_DIR}"
sudo mkdir -p "${LOKI_DATA_DIR}"
sudo chown -R "${LOKI_UID}:${LOKI_UID}" "${LOKI_DATA_DIR}"
echo "[setup] Loki data directory ready."

# --- Grafana ----------------------------------------------------------------

# Grafana runs as uid 472 inside the container.
GRAFANA_UID=472

echo "[setup] Creating Grafana data directory: ${GRAFANA_DATA_DIR}"
sudo mkdir -p "${GRAFANA_DATA_DIR}"
sudo chown -R "${GRAFANA_UID}:${GRAFANA_UID}" "${GRAFANA_DATA_DIR}"
echo "[setup] Grafana data directory ready."

# ---------------------------------------------------------------------------

echo ""
echo "[setup] Done. You can now run: docker compose up -d"
