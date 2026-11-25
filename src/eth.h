/* ParlorNet for the Apple IIe: the Uthernet II card (eth.c). */
#ifndef ETH_H
#define ETH_H

extern unsigned char eth_on;            /* 1 while the serial calls go to the card's TCP connection */
extern unsigned char cfg_conn;          /* 0 the Super Serial Card, 1 the Uthernet II (main.c) */
extern unsigned char cfg_eslot;
extern unsigned char cfg_ip[4], cfg_mask[4], cfg_gw[4], cfg_dns[4];
extern char cfg_host[32];
extern unsigned int cfg_port;

unsigned char eth_init(void);           /* 0 when no card answers in cfg_eslot */
unsigned char eth_tcp(const unsigned char *ip, unsigned int port);
void eth_close(void);
int eth_get(void);
void __fastcall__ eth_put(unsigned char c);
void eth_drop(void);

/* The W5100 itself, for the DNS lookup (parlor.c); w5100.s has the register accesses. */
#define W_S1 0x0500
void __fastcall__ w_slot(unsigned char slot);
void __fastcall__ w_at(unsigned int a);
unsigned char w_get(void);
void __fastcall__ w_mode(unsigned char v);
unsigned char w_mode_get(void);
unsigned char __fastcall__ w_reg(unsigned int a);
void w_setreg(unsigned int a, unsigned char v);
unsigned int w_get16(unsigned int a);
void __fastcall__ w_set16(unsigned int a, unsigned int v);
void __fastcall__ w_put(unsigned char v);
void w_cmd(unsigned int s, unsigned char c);
void w_read(unsigned int s, unsigned char *p, unsigned int n);
void w_write(unsigned int s, const unsigned char *p, unsigned int n);
unsigned int w_rsr(unsigned int s);

#endif
