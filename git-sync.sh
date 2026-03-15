cat > git-sync.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
MSG=${1:-"sync"}
git add .
git commit -m "$MSG" || echo "Nothing to commit"
git push
EOF
chmod +x git-sync.sh