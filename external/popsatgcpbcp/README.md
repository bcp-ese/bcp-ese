# POP-S-B and POPH-S-B source dependency

The baseline encodings are loaded from the public supplementary source of Faber,
Jabrayilov, and Mutzel:

- repository: <https://github.com/s6dafabe/popsatgcpbcp>;
- pinned commit: `8f19dbff4135e6cff9e4b147ebe8462603d5fe03`.

Run the preparation script from the repository root:

```sh
./prepare-pop-baseline-source.sh
```

This creates the ignored checkout `external/popsatgcpbcp/source` and a pinned Python
environment in `external/popsatgcpbcp/.venv`. Python 3.10 or newer is required because the
original source uses structural pattern matching. The checkout must remain detached at the
pinned commit and clean. The benchmark adapter imports the upstream
`POP_SAT_BCP` and `POPHyb_SAT_BCP` classes without changing their encoding formulas. It
supplies its own BCP-only driver because the upstream command-line entry point does not expose
the solver seed or explicitly select the descending linear search reported for BCP.

The executable shipped in the upstream repository is not used. Reported revision runs use a
separately built CaDiCaL 1.9.5 command-line executable, matching the SAT backend used by the
proposed implementations.

The official runner uses the prepared environment automatically. Set `BCP_PYTHON` only when
an equivalent pre-built environment is required.
