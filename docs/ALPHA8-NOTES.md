# Alpha 8 Process Explorer UX

Alpha 8 closes the remaining Process Explorer usability gap without adding a TUI framework or visual-heavy rendering.

## User-facing changes

- `/` opens a simple process search prompt.
- Search matches a case-insensitive process-name substring or PID substring.
- An empty search restores the full process list.
- Process Explorer shows current whole-machine CPU share per process when sampling succeeds.
- `c`, `m`, `p`, and `n` sort by CPU, memory, PID, and name.
- Sorting preserves the selected process by PID + start time instead of silently selecting a different process.
- CPU values come from a short 250 ms `/proc/<pid>/stat` delta against `/proc/stat`; no background daemon, cache, or thread is introduced.
- Search operates on the current in-memory process rows and does not shell out to `ps`, `grep`, or another command.

## UX rule

Process Explorer prioritizes discoverability over decoration. Search and sorting are visible near the table; destructive actions remain separated and explicitly confirmed.
