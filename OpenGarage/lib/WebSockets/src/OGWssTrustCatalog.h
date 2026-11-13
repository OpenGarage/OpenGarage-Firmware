#pragma once
#include "OGWssTrustStore.h"
#include "OGWssPublicAnchors.h"
namespace og_wss_trust {
inline Store &catalog() {static Store s(trustEntries,sizeof(trustEntries)/sizeof(trustEntries[0]));return s;}
}
