#define __STDC_WANT_LIB_EXT1__ 1

#include <signal.h>
#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <i86.h>
#include <graph.h>

#define SIGNATURE "CUTEHTV4"
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

typedef enum {
 dm_pause = 0,
 dm_fire = 0,
 dm_use,
 dm_change,
 dm_special = 7
} dm_t;

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

typedef struct {
 word_t pad[2]; // interrupt
 sbyte_t fwd;
 sbyte_t strafe;
 short turn;
 short cons;
 char chat;
 char btns;
} doomtic_t;

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
 char doom;
 char nl;
 char mult;
 char test;
 int vntic; 
 int vlim;

 union REGPACK regs;
 int ctr[4];
 int stat, hor, ver;
 int pitch;
 int i;
 sbyte_t dmweap;
 char sgn[sizeof(SIGNATURE)];
} tsrdata_t;

typedef void (_WCINTERRUPT *handler_t)();
static handler_t org_handler;

union {
  tsrdata_t data;
  doomtic_t tic;
} _u;

int calc(int x, int m) 
{
  int i, j, a, r, s;
  a = x < 0 ? -x : x;
  if(a <= 1) return x;
 
  for(j = a , s = r = 0; j > 0 ; j >>= 1) if(j&1) r+=1 ; else s+=1 ; 
  j = s + r - 1;
  if(j&1) j += r > s ? 1 : -1;
  for(i = 0, j >>= 1, r = s = 1 ; i < j ; i += 1) { r <<= 1 ; s <<= 1; }
  for(j <<= 1 ; i < j ; i += 1) s <<= 1;
  
  a = (a + s) / 2 / r;
  if(m) return a;
  return x < 0 ? -a : a;
}

void check_controls()
{
  memset(&_u.data.mvt, 0, sizeof(_u.data.mvt));

  if(!_u.data.reset) {
    _u.data.reset = 1;
    _u.data.ver = _u.data.vntic = 0;
    _u.data.vlim = 7 * _u.data.mult;
    for( _u.data.i = -1 ; _u.data.i < 3 ; _u.data.i += 1) {
      memset(&_u.data.regs, 0, sizeof(_u.data.regs));
      _u.data.regs.w.ax = 5;
      _u.data.regs.w.bx = _u.data.i;
      intr(0x33, &_u.data.regs);
    }

    _u.data.dmweap = 1;
    return;
  }
 
  for(_u.data.i = -1 ; _u.data.i < 3 ; _u.data.i += 1) {
    memset(&_u.data.regs, 0, sizeof(_u.data.regs));
    _u.data.regs.w.ax = 5;
    _u.data.regs.w.bx = _u.data.i;
    intr(0x33, &_u.data.regs);
    _u.data.ctr[_u.data.i + 1] = _u.data.regs.w.bx;
    _u.data.stat = _u.data.regs.w.ax;
  }

  memset(&_u.data.regs, 0, sizeof(_u.data.regs));
  _u.data.regs.w.ax = 0xb;
  intr(0x33, &_u.data.regs);
  _u.data.hor = _u.data.regs.w.cx;
  _u.data.ver = _u.data.regs.w.dx;

  if(!_u.data.doom) {
    if(_u.data.wheel)
    {
      if(_u.data.test) {
        _u.data.pitch += _u.data.ctr[0] > 0 ? 1 : _u.data.ctr[0] < 0 ? -1 : 0; 
      } else if(_u.data.flywheel) {
        _u.data.mvt.fly = -_u.data.ctr[0]; 
      } else if(_u.data.weap) {
        if(_u.data.ctr[0]) _u.data.mvt.btns |= (1 << btn_cycle);
      } else {
        if(_u.data.ctr[0] > 0) _u.data.mvt.btns |= (1 << btn_invp);
        if(_u.data.ctr[0] < 0) _u.data.mvt.btns |= (1 << btn_invn);
      }
    }
    if(_u.data.ctr[1] || _u.data.stat & 1) _u.data.mvt.btns |= (1 << btn_fire);
    if(_u.data.ctr[2] || _u.data.stat & 2) _u.data.mvt.btns |= (1 << btn_strafe);
    if(_u.data.flywheel || _u.data.weap) {
      _u.data.mvt.angle = _u.data.rev * 512;
      if(_u.data.stat & 4) {
        if(_u.data.rev > -8) _u.data.rev -= 1;
      } else {
        if(_u.data.rev < 0) _u.data.rev += 1;
      }
    } else {
      if(_u.data.ctr[3]) _u.data.mvt.btns |= (1 << btn_art);
    }
  } else if(_u.data.doom) {
    if(_u.data.ctr[1] || _u.data.stat & 1) _u.tic.btns |= (1 << dm_fire);
    if(_u.data.ctr[3]) {
      _u.data.dmweap = 2;
      _u.tic.btns |= (1 << dm_change) | (_u.data.dmweap << 3);
    } else if(_u.data.wheel && _u.data.ctr[0]) {
      for(_u.data.i = -_u.data.ctr[0] ; _u.data.i ; ) {
        if(_u.data.i > 0) {
          _u.data.dmweap++;
          _u.data.i--;
        } else {
          if(_u.data.dmweap == 0) _u.data.dmweap = 7;
          _u.data.dmweap--;
          _u.data.i++;  
        }
      }
      _u.data.dmweap %= 7;
      _u.tic.btns |= (1 << dm_change) | (_u.data.dmweap << 3);
    }
  }

  if(_u.data.tmr) {
    _u.data.now += 1;
    _u.data.dbl += _u.data.ctr[2];
    while(_u.data.dbl >= 2) {
      _u.data.dbl -= 2;
      _u.data.clk += 2;
      if(!_u.data.doom) _u.data.mvt.btns |= (1 << btn_use);
      else _u.tic.btns |= (1 << dm_use);
    }    
    if(_u.data.ctr[2] || _u.data.stat & 2) {
      _u.data.tmr = _u.data.now;
    } else if(_u.data.now - _u.data.tmr > 9) {
      if(!_u.data.doom) {
        if(_u.data.dbl + _u.data.clk >= 6) {
          _u.data.mvt.btns |= (1 << btn_pause);
          if(_u.data.flywheel) { _u.data.flywheel = 0 ; _u.data.weap = 1 ; }
          else if(_u.data.weap) { _u.data.flywheel = _u.data.weap = 0 ; }
          else { _u.data.flywheel = 1; _u.data.weap = 0; }
        }
        else if(_u.data.dbl + _u.data.clk >= 3) {
          _u.data.ver = _u.data.pitch = _u.data.vntic = 0;
          _u.data.mvt.btns |= (1 << btn_center);
        }
      } else if(_u.data.doom) {
        if(_u.data.dbl + _u.data.clk >= 15) _u.tic.btns = (1 << dm_pause) | (1 << dm_special);
      }
      _u.data.tmr = _u.data.dbl = _u.data.clk = 0;
    }  
  } else if(_u.data.ctr[2] || _u.data.stat & 2) {
    _u.data.tmr = _u.data.now = 1;
    _u.data.dbl = _u.data.ctr[2];
    _u.data.clk = 0;
  } 

  if(!_u.data.doom) {
    _u.data.i = _u.data.hor;
    if(_u.data.nl) {
      _u.data.i = _u.data.i > -1024 ? _u.data.i < 1024 ? _u.data.i : 1024 : -1024;
      _u.data.i = _u.data.i * calc(_u.data.i, 1);
    }
    else {
      if(_u.data.mvt.btns & (1 << btn_strafe)) {
        _u.data.i = _u.data.i * _u.data.sens / 10 * 2;
      } else {
        _u.data.i = _u.data.i * _u.data.sens / 10 * 8;
      }
    }  
    if(~_u.data.mvt.btns & (1 << btn_strafe)) 
      _u.data.i = -_u.data.i;
    _u.data.mvt.turn = _u.data.i;
  
    _u.data.i = _u.data.ver;
    if(_u.data.nl) {
      _u.data.i = calc(_u.data.i, 0);
      _u.data.i -= _u.data.vntic - _u.data.pitch;
      _u.data.vntic = _u.data.pitch - _u.data.i;
      _u.data.vntic = _u.data.vntic > -110 ? _u.data.vntic < 90 ? _u.data.vntic : 90 : -110;
      _u.data.i = _u.data.i > -_u.data.vlim ? _u.data.i < _u.data.vlim ? _u.data.i : _u.data.vlim : -_u.data.vlim;
    } else {
      _u.data.i = _u.data.i * _u.data.sens / 20;
    }
    if(!_u.data.test) _u.data.pitch -= _u.data.i;
    _u.data.pitch = _u.data.pitch > -110 ? _u.data.pitch < 90 ? _u.data.pitch : 90 : -110;
    _u.data.mvt.look = _u.data.pitch;

    if(_u.data.run) _u.data.mvt.btns |= (1 << btn_run);
  } else if(_u.data.doom) {
    if(_u.data.ctr[1] || _u.data.stat & 2) {
      _u.data.i = (_u.data.hor * _u.data.sens) / 10 * 2;
      _u.tic.strafe = _u.data.i >= -128 ? _u.data.i < 127 ? _u.data.i : 127 : -128;
    } else {
      _u.tic.turn = -(_u.data.hor * _u.data.sens) / 10 * 8;
    }
  }
}

char exec_cmd(char cmd)
{
  switch(cmd) {
    case cmd_uninst:
      _dos_setvect(_u.data.vector, (handler_t)0);
      _dos_freemem(_u.data.psp);
      return cmd; 
    default:
      ;
  }
  return 0;
}

void _WCINTERRUPT our_handler()
{
  char req = _u.data.req, resp = _u.data.resp, cnfm = _u.data.cnfm;
  _u.data.req = _u.data.resp = _u.data.cnfm = _u.data.ack = 0;

  check_controls();

  if(resp && resp == cnfm) _u.data.ack = exec_cmd(cnfm);
  else if(req) _u.data.resp = req;
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
  printf("Usage: CUTEHT [-v0x<vector>] [-u<address>] [-s<sensitivity>|-n] \n");
  printf("              [-a] [-f|w] [-k] [-d] [-t] [-m<multiplier>]\n");
  printf("                 where <vector> is from (60-67),\n");
  printf("                 <address> is memory block output by the driver before,\n");
  printf("                 <sensitivity> is from (1-50),\n");
  printf("                 \"n\" changes how sensitivity is applied,\n");
  printf("                 \"a\" is for always run,\n");
  printf("                 \"f\" is for flight mousewheel control\n");
  printf("                 \"w\" is for weapons mousewheel control\n");
  printf("                 \"d\" is for Doom/Ultimate Doom/Doom II/...\n\n");
  printf("                 \"k\" will generate " KILLTSR ".BAT in current directory\n");
  printf("                 \"t\" is to active test mode for developers\n");
  printf("                 <multiplier> can be 1, 2 or 5 and is for use with modded game\n\n");
  printf("Or:    TYPE RESPONSE | CUTEHT [...] > RESP.TMP\n");
  printf("       HERETIC @RESP.TMP\n");
  printf("if -k: " KILLTSR "\n");
  printf("\n (c) 2026, Ludictous_peridot\n");
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
  int i = 0, v = 0, j = 0, u = 0, s = 10, a = 0, r = 0, f = 0, w = 0, k = 0, d = 0, n = 0, m = 0, t = 0;
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
    else if(*argv[j] == 'd' || *argv[j] == 'D') d = 1; 
    else if(*argv[j] == 'n' || *argv[j] == 'N') n = 1; 
    else if(*argv[j] == 't' || *argv[j] == 'T') t = 1; 
    else if(*argv[j] == 'm' || *argv[j] == 'M') 
      sscanf(*(argv[j] + 1) || (j + 1 >= argc) ? argv[j] + 1 : argv[j += 1],
             "%d", &m);

    else if(*argv[j] == '?' || *argv[j] == 'h' || *argv[j] == 'h') help();
  }

  if(!u) goto inst;

  if(l) {
    remote = MK_FP(l >> 4, l & 0xf);
    _fmemcpy(&_u, remote, sizeof(_u.data));
    if(!strncmp(SIGNATURE, &_u.data.sgn, sizeof(SIGNATURE) - 1))
    {
      if(_u.data.vector >= 0x60 && _u.data.vector <= 0x67) {
        summary(_u.data.vector, u, r);
        
        _u.data.req = cmd_uninst;
        _fmemcpy(remote, &_u, sizeof(_u.data));
        intr(_u.data.vector, &regs);
        _fmemcpy(&_u, remote, sizeof(_u.data));
        if(_u.data.resp == cmd_uninst) {
          _u.data.cnfm = _u.data.resp;
          if(!r) printf("Uninstall command sent... ");
          _fmemcpy(remote, &_u, sizeof(_u.data));
          intr(_u.data.vector, &regs);
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

  memset(&_u, 0, sizeof(_u.data));
  strcpy(&(_u.data.sgn), SIGNATURE);
  _u.data.vector = v;
  _u.data.psp = _psp; 

  regs.w.ax = 0x11;
  intr(0x33, &regs);
  _u.data.wheel = regs.w.ax == 0x574d && regs.w.cx & 1;
  if(_u.data.wheel) {
    if(!r) printf("Mouse wheel detected.");
    if(t && !d) {
      if(!r) printf(" Test mode.");
    } else if(f && !d) {
      if(!r) printf(" Wheel controls flight.");
    }
    else if(w) {
      if(!r) printf(" Wheel controls weapons.");
    }
    printf("\n");
  }
  
  if(d) {
    _u.data.doom = d;
  }
  else if(t) {
    _u.data.test = t;
  } else if(f) {
    _u.data.flywheel = f;
  } else if(w) {
    _u.data.weap = w;
  }

  if(s < 1 || s > 50) s = 10;
  if(m != 1 && m != 2) m = 5;
  if(!r) printf("Sensitivity: %d", s);
  _u.data.sens = s;
  if(!r) {
    if(n && !d) printf(" (non linear)"); 
    if(m && m != 5 && !d) printf(". Modded look step"); 
    if(a && !d) printf(". Always run on");
    printf(".\n"); 
  }
  if(!d) {
    _u.data.run = a;
    _u.data.nl = n;
    _u.data.mult = m;
  }

  segread(&segs);
  l = ((long)segs.ds << 4) + (unsigned)(&_u);
  if(k) makekill(argc, argv, l);
  if(!r) {
    printf("Memory block: %ld (%x:%x)\n", l, segs.ds, &_u);
    printf("%s -nomouse -%s %ld\n",
           d ? "DOOM" : "HERETIC", d ? "control" : "externdriver", l);
    if(k) printf("then run " KILLTSR " to remove driver.\n");
  } else {
    printf("-nomouse\n-%s %ld\n", d ? "control" : "externdriver", l);
  }

  _dos_setvect(v, our_handler);
  l = (long)segs.ds - _psp;
  l = ((l << 4) + (unsigned)(&_u) + sizeof(_u.data) - 1) / 16 + 1;
  fclose(stdin); fclose(stdout); fclose(stderr);
  remote = MK_FP(_psp, 0x2c);
  _fmemcpy(&i, remote, sizeof(unsigned short));
  _dos_freemem((unsigned) i);
  _dos_keep(0, l);
}

