#pragma once
#include <CertStoreBearSSL.h>
#include <new>
#include <umm_malloc/umm_heap_select.h>

namespace og_wss_trust {
struct Entry {
    const uint8_t *dn, *key;
    uint16_t length, first, second;
    uint8_t kind;
};
// Heap workspace is allocated in DRAM before connect(), never in callbacks.
// Entries contain only public keys and hashed subject DNs, all in flash.
class Store : public BearSSL::CertStoreBase {
    struct Workspace {
        br_x509_trust_anchor anchor;
        uint8_t dn[32], key[515];
    };
#ifdef ESP8266
    static_assert(sizeof(Workspace)==580,"Review trust workspace RAM growth");
#endif
    const Entry *entries_;
    size_t count_;
    Workspace *workspace_=nullptr;
    bool claimed_=false;
 public:
    Store(const Entry *entries,size_t count):entries_(entries),count_(count) {}
    Store(const Store &)=delete;
    Store &operator=(const Store &)=delete;
    ~Store() override { delete workspace_; }
    bool begin() {
        if (!workspace_) { HeapSelectDram heap; workspace_=new(std::nothrow) Workspace{}; }
        return workspace_!=nullptr;
    }
    // Only after the previous client/validator has been destroyed.
    void resetClaimForConnection() { claimed_=false; }
    // Core 3.1.2 discards its X509 validator before connect() returns;
    // renegotiation is disabled. No callback can then refer to this workspace.
    void endHandshake() { claimed_=false; delete workspace_;workspace_=nullptr; }
    bool ready() const { return workspace_!=nullptr; }
    bool claimed() const { return claimed_; }
    void installCertStore(br_x509_minimal_context *ctx) override {
        br_x509_minimal_set_dynamic(ctx,this,lookup,release);
    }
    static const br_x509_trust_anchor *lookup(void *opaque,void *hash,size_t len) {
        auto *s=static_cast<Store*>(opaque);
        if (!s || !s->ready() || s->claimed_ || !hash || len!=32) return nullptr;
        for (size_t i=0;i<s->count_;++i) {
            Entry entry; memcpy_P(&entry,s->entries_+i,sizeof(entry));
            if (memcmp_P(hash,entry.dn,32)) continue;
            if (!entry.length || entry.length>sizeof(s->workspace_->key)) return nullptr;
            if (entry.kind==BR_KEYTYPE_RSA) {
                if (!entry.first || !entry.second || unsigned(entry.first)+entry.second!=entry.length) return nullptr;
            } else if (entry.kind==BR_KEYTYPE_EC) {
                const unsigned expected=entry.first==23?65:entry.first==24?97:entry.first==25?133:0;
                if (entry.length!=expected || entry.second) return nullptr;
            } else return nullptr;
            auto &w=*s->workspace_;
            memset(&w.anchor,0,sizeof(w.anchor));
            memcpy(w.dn,hash,32);memcpy_P(w.key,entry.key,entry.length);
            w.anchor.dn.data=w.dn;w.anchor.dn.len=32;w.anchor.flags=BR_X509_TA_CA;
            w.anchor.pkey.key_type=entry.kind;
            if (entry.kind==BR_KEYTYPE_RSA) {
                w.anchor.pkey.key.rsa.n=w.key;w.anchor.pkey.key.rsa.nlen=entry.first;
                w.anchor.pkey.key.rsa.e=w.key+entry.first;w.anchor.pkey.key.rsa.elen=entry.second;
            } else {
                w.anchor.pkey.key.ec.curve=entry.first;
                w.anchor.pkey.key.ec.q=w.key;w.anchor.pkey.key.ec.qlen=entry.length;
            }
            s->claimed_=true;return &w.anchor;
        }
        return nullptr;
    }
    static void release(void *opaque,const br_x509_trust_anchor *anchor) {
        auto *s=static_cast<Store*>(opaque);
        if (s && s->workspace_ && anchor==&s->workspace_->anchor) s->claimed_=false;
    }
};
}
