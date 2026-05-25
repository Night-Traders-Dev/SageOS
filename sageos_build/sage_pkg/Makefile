.PHONY: install clean sync-deps help

help:
	@echo "SagePkg Makefile"
	@echo "Available targets:"
	@echo "  install    - Install sagepkg to ~/.sagepkg/bin and update PATH"
	@echo "  sync-deps  - Propagate lib/json.sage to all bundled copies"
	@echo "  clean      - Remove any local build artifacts (currently none)"

install:
	@bash install.sh

# lib/json.sage is the single source of truth for the JSON library.
# Run this after editing lib/json.sage to keep all bundled copies in sync.
sync-deps:
	@echo "Syncing lib/json.sage -> json.sage ..."
	@{ head -4 json.sage; tail -n +2 lib/json.sage; } > /tmp/_sagepkg_json_sync && mv /tmp/_sagepkg_json_sync json.sage
	@echo "Syncing lib/json.sage -> packages/sagepkg/universal/json.sage ..."
	@{ head -4 packages/sagepkg/universal/json.sage; tail -n +2 lib/json.sage; } > /tmp/_sagepkg_json_sync && mv /tmp/_sagepkg_json_sync packages/sagepkg/universal/json.sage
	@echo "Done."

clean:
	@echo "Nothing to clean."
