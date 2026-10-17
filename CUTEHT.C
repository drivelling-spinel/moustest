#define __STDC_WANT_LIB_EXT1__ 1

#include <signal.h>
#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <i86.h>
#include <graph.h>

#define SIGNATURE "CUTEHTV2"
#define KILLTSR "KILLTSR"

typedef enum {
 cmd_uninst = 0x55
} cmd_t;

typedef enum {
 btn_fire = 0,
 btn_use,
 btn_run,
 btn_strafe,
 btn_automap,
 btn_invp,
 btn_invn,
 btn_art,
 btn_drop,
 btn_center,
 btn_pause,
 btn_cycle
} btn_t;

typedef signed char sbyte_t;
typedef unsigned short word_t;

typedef struct
{
 sbyte_t fwd;
 sbyte_t strafe;
 short turn;
 short angle;
 sbyte_t look;
 sbyte_t fly;
 word_t btns;
} controldata_t;

typedef struct 
{
 word_t vector; 
 controldata_t mvt; 

 word_t psp;

 char run; 
 char wheel;
 char flywheel;
 char weap;
 sbyte_t rev;
 int dbl;
 int clk;
 int tmr;
 int now;
 char sens;
 char reset;
 char req;
 char resp;
 char cnfm;
 char ack;

 union REGPACK regs;
 int ctr[4];
 int stat, hor, ver;
 int pitch;
 int i;
 char sgn[sizeof(SIGNATURE)];
} tsrdata_t;

typedef void (_WCINTERRUPT *handler_t)();
static handler_t org_handler;

tsrdata_t data;

void check_controls()
{
  memset(&data.mvt, 0, sizeof(data.mvt));

  if(!data.reset) {
    data.reset = 1;
    data.ver = 0;
    for( data.i = -1 ; data.i < 3 ; data.i += 1) {
      memset(&data.regs, 0, sizeof(data.regs));
      data.regs.w.ax = 5;
      data.regs.w.bx = data.i;
      intr(0x33, &data.regs);
    }
    return;
  }
 
  for( data.i = -1 ; data.i < 3 ; data.i += 1) {
    memset(&data.regs, 0, sizeof(data.regs));
    data.regs.w.ax = 5;
    data.regs.w.bx = data.i;
    intr(0x33, &data.regs);
    data.ctr[data.i + 1] = data.regs.w.bx;
    data.stat = data.regs.w.ax;
  }

  memset(&data.regs, 0, sizeof(data.regs));
  data.regs.w.ax = 0xb;
  intr(0x33, &data.regs);
  data.hor = data.regs.w.cx;
  data.ver = data.regs.w.dx;

  if(data.wheel)
  {
    if(data.flywheel) {
      data.mvt.fly = -data.ctr[0]; 
    } else if(data.weap) {
      if(data.ctr[0]) data.mvt.btns |= (1 << btn_cycle);
    } else {
      if(data.ctr[0] > 0) data.mvt.btns |= (1 << btn_invp);
      if(data.ctr[0] < 0) data.mvt.btns |= (1 << btn_invn);
    }
  }
  if(data.ctr[1] || data.stat & 1) data.mvt.btns |= (1 << btn_fire);
  if(data.ctr[2] || data.stat & 2) data.mvt.btns |= (1 << btn_strafe);
  if(data.flywheel || data.weap) {
    data.mvt.angle = data.rev * 512;
    if(data.stat & 4) {
      if(data.rev > -8) data.rev -= 1;
    } else {
      if(data.rev < 0) data.rev += 1;
    }
  } else {
    if(data.ctr[3]) data.mvt.btns |= (1 << btn_art);
  }

  if(data.tmr) {
    data.now += 1;
    data.dbl += data.ctr[2];
    while(data.dbl >= 2) {
      data.dbl -= 2;
      data.clk += 2;
      data.mvt.btns |= (1 << btn_use);
    }    
    if(data.ctr[2] || data.stat & 2) {
      data.tmr = data.now;
    } else if(data.now - data.tmr > 9) {
      if(data.dbl + data.clk >= 15) data.mvt.btns |= (1 << btn_pause);
      else if(data.dbl + data.clk >= 3) {
        data.ver = data.pitch = 0;
        data.mvt.btns |= (1 << btn_center);
      }
      data.tmr = data.dbl = data.clk = 0;
    }  
  } else if(data.ctr[2] || data.stat & 2) {
    data.tmr = data.now = 1;
    data.dbl = data.ctr[2];
    data.clk = 0;
  } 

  if(data.mvt.btns & (1 << btn_strafe)) {
    data.mvt.turn = (data.hor * data.sens) / 10 * 2;
  } else {
    data.mvt.turn = -(data.hor * data.sens) / 10 * 8;
  }
  data.pitch -= (data.ver * data.sens) / 20;
  data.pitch = data.pitch < 90 ? data.pitch > - 110 ? data.pitch : -110 : 90;
  data.mvt.look = data.pitch;

  if(data.run) data.mvt.btns |= (1 << btn_run);
}

char exec_cmd(char cmd)
{
  switch(cmd) {
    case cmd_uninst:
      _dos_setvect(data.vector, (handler_t)0);
      _dos_freemem(data.psp);
      return cmd; 
    default:
      ;
  }
  return 0;
}

void _WCINTERRUPT our_handler()
{
  char req = data.req, resp = data.resp, cnfm = data.cnfm;
  data.req = data.resp = data.cnfm = data.ack = 0;

  check_controls();

  if(resp && resp == cnfm) data.ack = exec_cmd(cnfm);
  else if(req) data.resp = req;
}

void doneprog()
{
  exit(0);
}

char * progname(int argc, char ** argv)
{
  char * progname = argv[0] + strlen(argv[0]);
  while(progname > argv[0]) {
    progname -= 1;
    if(*progname == '\\' || *progname == '/') {
      progname +=1 ; break;
    }
  }
  return progname;
}

void help()
{
  printf("\nHeretic CuteMouse Driver.\n");
  printf("Usage: CUTEHT [-v0x<vector>] [-u<address>] [-s<sensitivity>] [-a] [-f|w] [-k]\n");
  printf("                 where value is from (60-67),\n");
  printf("                 address is memory block output by the driver before,\n");
  printf("                 sensitivity is from 1 to 50,\n");
  printf("                 \"k\" will generate " KILLTSR ".BAT in current directory\n");
  printf("                 \"a\" is for always run,\n");
  printf("                 \"f\" is for flight mousewheel control\n");
  printf("                 \"w\" is for weapons mousewheel control\n\n");
  printf("Or:    TYPE RESPONSE | CUTEHT [...] > RESP.TMP\n");
  printf("       HERETIC @RESP.TMP\n");
  printf("if -k: " KILLTSR "\n");
  printf("\n");
  doneprog();
}

void summary(int v, int u, int r)
{
  if(r) return;
  printf("%s Driver\n", u ? "Removing" : "Initializing");
  printf("Vector 0x%x\n", v);
}

void makekill(int argc, char ** argv, long mem)
{
  FILE * batch = fopen(KILLTSR ".BAT", "wt");
  fprintf(batch, "@ECHO OFF\n%s /u %ld\n", progname(argc, argv), mem);
  fclose(batch);
}

void main(int argc, char ** argv)
{
  int i = 0, v = 0, j = 0, u = 0, s = 10, a = 0, r = 0, f = 0, w = 0, k = 0;
  long l = 0;
  handler_t org = 0;
  struct SREGS segs;
  tsrdata_t _WCFAR * remote;
  union REGPACK regs;

  i = ftell(stdin);
  if(!fseek(stdin, 0, SEEK_END) && i != ftell(stdin)) r = 1;
  rewind(stdin);
  while(r && !feof(stdin)) {
    char buf[100] = {0};
    char * s = gets_s(buf, 99);
    if(s) {
      puts(s);
    }
  }

  for(j = 1 ; j < argc ; j++) {
    while(*argv[j] == '-' || *argv[j] == '/' || *argv[j] == ' ') argv[j] += 1;
    if(*argv[j] == 'v' || *argv[j] == 'V') 
      sscanf(*(argv[j] + 1) || (j + 1 >= argc) ? argv[j] + 1 : argv[j += 1],
             "0x%x", &v);
    else if(*argv[j] == 'u' || *argv[j] == 'U') 
      sscanf(*(argv[j] + 1) || (j + 1 >= argc) ? argv[j] + 1 : argv[j += 1],
             "%ld", &l), u = 1;
    else if(*argv[j] == 's' || *argv[j] == 'S') 
      sscanf(*(argv[j] + 1) || (j + 1 >= argc) ? argv[j] + 1 : argv[j += 1],
             "%d", &s);
    else if(*argv[j] == 'a' || *argv[j] == 'A') a = 1;
    else if(*argv[j] == 'f' || *argv[j] == 'F') f = 1; 
    else if(*argv[j] == 'w' || *argv[j] == 'W') w = 1; 
    else if(*argv[j] == 'k' || *argv[j] == 'K') k = 1; 
    else if(*argv[j] == '?' || *argv[j] == 'h' || *argv[j] == 'h') help();
  }

  if(!u) goto inst;

  if(l) {
    remote = MK_FP(l >> 4, l & 0xf);
    _fmemcpy(&data, remote, sizeof(data));
    if(!strncmp(SIGNATURE, &data.sgn, sizeof(SIGNATURE) - 1))
    {
      if(data.vector >= 0x60 && data.vector <= 0x67) {
        summary(data.vector, u, r);
        
        data.req = cmd_uninst;
        _fmemcpy(remote, &data, sizeof(data));
        intr(data.vector, &regs);
        _fmemcpy(&data, remote, sizeof(data));
        if(data.resp == cmd_uninst) {
          data.cnfm = data.resp;
          if(!r) printf("Uninstall command sent... ");
          _fmemcpy(remote, &data, sizeof(data));
          intr(data.vector, &regs);
          doneprog();
        }
      }
    }
  }

  fprintf(stderr, "Unable to confirm driver is loaded; backing out\n");
  doneprog();
inst:
  for(i = 0x60 ; !v && i <= 0x67 ; i += 1) {
      if(!_dos_getvect(i)) {
        v = i ; break;
      }
  }

  if(v >= 0x60 && v <= 0x67) {
    summary(v, u, r);
    org = _dos_getvect(v);
    if(org) {
      fprintf(stderr, "Vector 0x% not free\n", v);
      doneprog();
    }
  } else if(v) {
    summary(v, u, r);
    fprintf(stderr, "Vector 0x%x not in (0x60-0x67)\n", v);
    doneprog();
  } else {
    fprintf(stderr, "Unable to locate a free vector from (0x60-0x67)\n");
    doneprog();
  }

  memset(&data, 0, sizeof(data));
  strcpy(&(data.sgn), SIGNATURE);
  data.vector = v;
  data.psp = _psp; 

  regs.w.ax = 0x11;
  intr(0x33, &regs);
  data.wheel = regs.w.ax == 0x574d && regs.w.cx & 1;
  if(data.wheel) {
    if(!r) printf("Mouse wheel detected.");
    if(f) {
      if(!r) printf(" Wheel controls flight.");
    }
    else if(w) {
      if(!r) printf(" Wheel controls weapons.");
    }
    printf("\n");
  }

  if(f) {
    data.flywheel = f;
  }
  else if(w) {
    data.weap = w;
  }

  if(s < 1 || s > 50) s = 10;
  if(!r) printf("Sensitivity: %d.", s);
  data.sens = s;
  if(!r) printf("%s\n", a ? " Always run on." : "");
  data.run = a;
  data.flywheel = f;

  segread(&segs);
  l = ((long)segs.ds << 4) + (unsigned)(&data);
  if(k) makekill(argc, argv, l);
  if(!r) {
    printf("Memory block: %ld (%x:%x)\n", l, segs.ds, &data);
    printf("HERETIC -nomouse -externdriver %ld\n", l);
    if(k) printf("then run " KILLTSR " to remove driver.\n");
  } else {
    printf("-nomouse\n-externdriver %ld\n", l);
  }

  _dos_setvect(v, our_handler);
  l = (long)segs.ds - _psp;
  l = ((l << 4) + (unsigned)(&data) + sizeof(data) - 1) / 16 + 1;
  fclose(stdin); fclose(stdout); fclose(stderr);
  remote = MK_FP(_psp, 0x2c);
  _fmemcpy(&i, remote, sizeof(unsigned short));
  _dos_freemem((unsigned) i);
  _dos_keep(0, l);
}

