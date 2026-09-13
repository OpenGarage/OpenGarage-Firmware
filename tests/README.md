# Focused native tests

From the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_distance_consensus.cpp -o /tmp/og-test-distance-consensus
/tmp/og-test-distance-consensus
g++ -std=c++11 -Wall -Wextra -Werror tests/test_secplus2_identity.cpp -o /tmp/og-test-secplus2-identity
/tmp/og-test-secplus2-identity
```

These test the production distance-consensus and identity lifecycle helpers without Arduino dependencies.
They do not replace sensor checks on hardware.
