/* ParlorNet for the Apple IIgs: the toolbox and GS/OS (gs.s, gsos.c) and Marinetti (mt.c). */
#ifndef GS_H
#define GS_H

/* A toolbox call: tl_nin words of parameters in tl_in (in the order they are pushed; a long is its high
   word, then its low word), tl_nout words of results back in tl_out (the first word off the stack in
   tl_out[0]). Returns the error code. */
extern unsigned int tl_nin, tl_nout, tl_err;
extern unsigned int tl_in[12], tl_out[4];
unsigned int __fastcall__ tl_call(unsigned int fn);

/* A GS/OS (class 1) call with its parameter block in our bank. Returns the error code. */
unsigned int __fastcall__ gs_call(unsigned int num, void *parms);

extern unsigned int gs_bank;            /* our bank, for long addresses of our data */
extern unsigned int gs_uid;             /* our Memory Manager user ID */
void __fastcall__ gs_video(unsigned char on);

/* Put a pointer to our data into tl_in at i and i + 1. */
#define TL_PTR(i, p) (tl_in[i] = gs_bank, tl_in[(i) + 1] = (unsigned int)(p))

/* Marinetti (mt.c), behind the Uthernet II's interface (eth.h). */
unsigned int mt_up(void);               /* Marinetti started and connected to the network; 0, or an error */
unsigned char mt_dns(const char *name, unsigned char *ip);  /* 1 when found */
void mt_end(void);                      /* before quitting */

#endif
