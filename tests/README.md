# Focused native tests

From the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_distance_consensus.cpp -o /tmp/og-test-distance-consensus
/tmp/og-test-distance-consensus
g++ -std=c++11 -Wall -Wextra -Werror tests/test_secplus2_identity.cpp -o /tmp/og-test-secplus2-identity
/tmp/og-test-secplus2-identity
g++ -std=c++11 -Wall -Wextra -Werror tests/test_health_stats.cpp -o /tmp/og-test-health-stats
/tmp/og-test-health-stats
g++ -std=c++11 -Wall -Wextra -Werror tests/test_body_device_key.cpp -o /tmp/og-test-body-key
/tmp/og-test-body-key
node tests/test_identity_button.mjs
g++ -std=c++11 -Wall -Wextra -Werror tests/test_audio_timing.cpp -o /tmp/og-test-audio
/tmp/og-test-audio
```

These test production distance-consensus, health, identity lifecycle and exact POST-body authentication helpers without Arduino dependencies. The Node test exercises the confirmation button's actual JavaScript, request body, cancellation and placement.
They do not replace sensor checks on hardware.
