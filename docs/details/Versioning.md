The compiler uses [Semantic Versioning](https://semver.org/).

The command `btb --version` will tell you the semver version. It also provides the git commit hash which you can `git checkout` after having cloned the BTB repository to see the exact source code the semver version uses.

Currently the compiler is *unstable* and in *pre-release* state (indicated by zero as major number, i.e. `0.2.0`). It is our intention that incremented patch numbers (`0.2.0` -> `0.2.1`) are *mostly* compatible while incremented minor numbers (`0.2.1` -> `0.3.0`) are known to have breaking changes. We don't follow semver in *pre-release* state.
