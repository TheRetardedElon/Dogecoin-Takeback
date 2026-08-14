libmdbx (amalgamated) — local chainstate/index backend for Core Pro.

Source: official amalgamated distribution (v0.14.3).
License: Apache 2.0 (see LICENSE).
This is a local KV engine only. It does not change Dogecoin consensus or P2P.

Enable with -dbengine=mdbx on a new/empty datadir (or Fast Sync into a fresh dir).
Existing LevelDB folders are refused (ENGINE stamp / CURRENT file).
