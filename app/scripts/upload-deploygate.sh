#!/bin/bash
# DeployGate に上げる。
#
#   ./scripts/upload-deploygate.sh build/CardCase.ipa "何を直したか"
#
# 先に次の 2 つを環境変数に入れておくこと。値はリポジトリに置かない。
#
#   DEPLOYGATE_USER       DeployGate のユーザー名
#   DEPLOYGATE_API_TOKEN  https://deploygate.com/settings で発行する API key

set -euo pipefail

file=${1:-}
message=${2:-}

if [ -z "$file" ] || [ ! -f "$file" ]; then
  echo "使い方: $0 <ipa か apk> [メッセージ]" >&2
  exit 1
fi
if [ -z "${DEPLOYGATE_USER:-}" ] || [ -z "${DEPLOYGATE_API_TOKEN:-}" ]; then
  echo "DEPLOYGATE_USER と DEPLOYGATE_API_TOKEN を環境変数に入れること。" >&2
  exit 1
fi

echo "==> $file を上げる"
response=$(curl -sS \
  -F "file=@$file" \
  -F "token=$DEPLOYGATE_API_TOKEN" \
  -F "message=$message" \
  "https://deploygate.com/api/users/$DEPLOYGATE_USER/apps")

if echo "$response" | grep -q '"error": *true'; then
  echo "失敗した:" >&2
  echo "$response" >&2
  exit 1
fi
echo "$response"
echo
echo "上げた。DeployGate の配布ページから入れられる。"
