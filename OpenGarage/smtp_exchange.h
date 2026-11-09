#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace og_smtp {
enum class Delivery { SENT, REFUSED, FAILED };
inline bool memory_ready(size_t free, size_t block) {return free>=22000 && block>=12288;}
inline bool header_safe(const char *s) {
    if (!s) return false;
    for (; *s; ++s) if ((unsigned char)*s < 32 || (unsigned char)*s == 127) return false;
    return true;
}
inline bool credential_safe(const char *s) { return s && strlen(s) <= 189; }
struct Result { bool accepted; int code; const char *stage; };

// Clock supplies now()/idle(). No heap allocation and no per-byte deadline reset.
template<class Client, class Clock> class Exchange {
    Client &c; Clock &clock; uint32_t started;
    bool expired() const { return uint32_t(clock.now()-started) >= 30000; }
public:
    Exchange(Client &client, Clock &timer, uint32_t start):c(client),clock(timer),started(start) {}
    uint32_t remaining() const {
        uint32_t used=uint32_t(clock.now()-started);return used>=30000 ? 0 : 30000-used;
    }
    bool write(const char *p, size_t n) {
        if (expired()) return false;
        c.setTimeout(remaining()<5000 ? remaining():5000);
        return c.write(reinterpret_cast<const uint8_t*>(p),n)==n && !expired();
    }
    bool text(const char *p) {return write(p,strlen(p));}
    bool line(const char *p) {return text(p)&&text("\r\n");}
    // Maximum 512 bytes per line including CRLF, and 32 lines per reply.
    // SMTP requires CRLF; bare LF or CR not followed by LF is rejected.
    int reply() {
        uint32_t begin=clock.now(); int first=0;
        for (unsigned lines=0;lines<32;++lines) {
            char prefix[4]={};size_t count=0;bool cr=false;
            for (;;) {
                if (expired() || uint32_t(clock.now()-begin)>=10000) return -1;
                if (!c.available()) {if(!c.connected())return -2;clock.idle();continue;}
                int ch=c.read();if(ch<0){clock.idle();continue;}
                if (++count>512) return -3;
                if(count<=4)prefix[count-1]=char(ch);
                if(ch=='\n') {if(!cr) return -4;break;}
                if(cr) return -4;
                cr=ch=='\r';
            }
            if(expired() || uint32_t(clock.now()-begin)>=10000)return -1;
            if(count<5 || prefix[0]<'2'||prefix[0]>'5'||prefix[1]<'0'||prefix[1]>'9'||prefix[2]<'0'||prefix[2]>'9')return -4;
            int code=(prefix[0]-'0')*100+(prefix[1]-'0')*10+prefix[2]-'0';
            if(first && code!=first)return -4;
            first=code;
            if(prefix[3]==' ' || (count==5&&prefix[3]=='\r'))return code;
            if(prefix[3]!='-')return -4;
        }
        return -5;
    }
    bool command(const char *p,int expected) {return line(p)&&reply()==expected;}
    bool __attribute__((noinline)) auth64(const char *s) {
        static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        // Batches writes instead of producing a TLS record per Base64 quartet.
        char out[128];size_t used=0,n=strlen(s);
        for(size_t i=0;i<n;i+=3){unsigned a=(unsigned char)s[i],b=i+1<n?(unsigned char)s[i+1]:0,d=i+2<n?(unsigned char)s[i+2]:0;
            char q[4]={alphabet[a>>2],alphabet[((a&3)<<4)|(b>>4)],i+1<n?alphabet[((b&15)<<2)|(d>>6)]:'=',i+2<n?alphabet[d&63]:'='};
            if(used+4>sizeof(out)){if(!write(out,used))return false;used=0;}
            memcpy(out+used,q,4);used+=4;
        }
        return write(out,used)&&text("\r\n");
    }
    bool __attribute__((noinline)) subject(const char *s) {
        bool ascii=true;for(const char*p=s;*p;++p)if((unsigned char)*p>=128)ascii=false;
        if(!text("Subject: "))return false;
        if(ascii)return line(s);
        // <=39 UTF-8 bytes per encoded word; don't split a UTF-8 codepoint.
        while(*s){size_t n=strlen(s);if(n>39){n=39;while(n&&((unsigned char)s[n]&0xc0)==0x80)--n;if(!n)return false;}
            char chunk[40];memcpy(chunk,s,n);chunk[n]=0;
            if(!text("=?UTF-8?B?"))return false;
            // auth64 appends CRLF, so use its batching via an in-place bounded encoder here.
            static const char a[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            char enc[52];size_t k=0;
            for(size_t i=0;i<n;i+=3){unsigned x=(unsigned char)chunk[i],y=i+1<n?(unsigned char)chunk[i+1]:0,z=i+2<n?(unsigned char)chunk[i+2]:0;
                enc[k++]=a[x>>2];enc[k++]=a[((x&3)<<4)|(y>>4)];enc[k++]=i+1<n?a[((y&15)<<2)|(z>>6)]:'=';enc[k++]=i+2<n?a[z&63]:'=';}
            if(!write(enc,k)||!text("?="))return false;
            s+=n;
            if(*s&&!text("\r\n "))return false;
        }
        return text("\r\n");
    }
    bool __attribute__((noinline)) body(const char *s,bool qp) {
        char out[128];size_t used=0,col=0;bool start=true;
        auto flush=[&](){bool ok=write(out,used);used=0;return ok;};
        auto emit=[&](char ch){if(used==sizeof(out)&&!flush())return false;out[used++]=ch;return true;};
        const char *hex="0123456789ABCDEF";
        for(;*s;++s){unsigned char ch=*s;
            if(ch=='\r'||ch=='\n'){if(ch=='\r'&&s[1]=='\n')++s;if(!emit('\r')||!emit('\n'))return false;col=0;start=true;continue;}
            bool escaped=qp&&(ch<33||ch>126||ch=='=');size_t width=escaped?3:1;
            if(col+width>72){if(qp&&!emit('='))return false;if(!emit('\r')||!emit('\n'))return false;col=0;start=true;}
            if(start&&ch=='.'&&!emit('.'))return false;
            if(escaped){if(!emit('=')||!emit(hex[ch>>4])||!emit(hex[ch&15]))return false;}
            else if(!emit(ch))return false;
            col+=width;start=false;
        }
        return emit('\r')&&emit('\n')&&flush();
    }
    Result send(const char *ip,const char *user,const char *password,const char *from,const char *to,const char *title,const char *message,const char *date,const char *id) {
        int code=reply();if(code!=220)return {false,code,"greeting"};
        if(!text("EHLO [")||!text(ip)||!line("]"))return {false,-1,"EHLO write"};
        code=reply();
        if(code==500 || code==502) {
            if(!text("HELO [")||!text(ip)||!line("]"))return {false,-1,"HELO write"};
            code=reply();
        }
        if(code!=250)return {false,code,"EHLO"};
        if(!line("AUTH LOGIN"))return {false,-1,"authentication write"};
        code=reply();if(code!=334)return {false,code,"username challenge"};
        if(!auth64(user))return {false,-1,"username write"};
        code=reply();if(code!=334)return {false,code,"password challenge"};
        if(!auth64(password))return {false,-1,"password write"};
        code=reply();if(code!=235)return {false,code,"authentication"};
        if(!text("MAIL FROM:<")||!text(from)||!line(">"))return {false,-1,"sender write"};
        code=reply();if(code!=250)return {false,code,"sender"};
        if(!text("RCPT TO:<")||!text(to)||!line(">"))return {false,-1,"recipient write"};
        code=reply();if(code!=250&&code!=251)return {false,code,"recipient"};
        if(!line("DATA"))return {false,-1,"DATA write"};
        code=reply();if(code!=354)return {false,code,"DATA"};
        if(!text("From: <")||!text(from)||!line(">")||!text("To: <")||!text(to)||!line(">")||!subject(title))return {false,-1,"headers"};
        if(date&&*date){if(!text("Date: ")||!line(date))return {false,-1,"date"};}
        if(id && *id && (!text("Message-ID: <")||!text(id)||!line(">")))return {false,-1,"Message-ID"};
        if(!line("MIME-Version: 1.0")||!line("Content-Type: text/plain; charset=UTF-8"))return {false,-1,"MIME"};
        bool qp=false;size_t column=0;
        for(const char*p=message;*p;++p) {
            if(*p=='\r'||*p=='\n')column=0;else if(++column>72)qp=true;
            if((unsigned char)*p>=128 || ((unsigned char)*p<32&&*p!='\r'&&*p!='\n'&&*p!='\t'))qp=true;
        }
        if(!line(qp?"Content-Transfer-Encoding: quoted-printable":"Content-Transfer-Encoding: 7bit")||!line("")||!body(message,qp)||!line("."))return {false,-1,"body"};
        code=reply();if(code!=250)return {false,code,"acceptance"};
        // Accepted DATA is final. QUIT is best-effort, never a delivery failure.
        line("QUIT");return {true,250,"accepted"};
    }
};
}
