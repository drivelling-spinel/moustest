#include <signal.h>
#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <i86.h>
#include <graph.h>

#define VERSION "V1"

static int has_exited = 0;
static int verbosity = 0;
static int irq = 0;
static int port = 0;
static int count = 0;
static int thorough = 0;

static int org_mask = 0;
static int org_ier = 0;
static int org_mcr = 0;
static int org_dll = 0;
static int org_dlm = 0;
static int org_lcr = 0;
static int org_col = -1;
static struct rccoord org_pos;

typedef void (_WCINTERRUPT *handler_t)();
static handler_t org_handler;

void window()
{
  static int w = 0;
  int i = 0;
  if(w) return;
  w = 1;

  for(i = 1 ; i <= 8 ; i++) {
    _settextposition(i, 1);
    _outtext("                           ");
  }

  _settextcolor(4);
  _settextposition(1, 1);
  _outtext("GENERIC INFORMATION");
  _settextcolor(14);
  _settextposition(2, 1);
  _outtext("total events:");
  _settextposition(3, 1);
  _outtext("raw: ");
   
  _settextcolor(4);
  _settextposition(4, 1);
  _outtext("SPECIFIC PROTOCOL");
}

int max_raw = 7;
int raw[7];


void generic()
{
  char buffer[32];
  int i;
  _settextcolor(14);
  _settextposition(2, 18);
  sprintf(buffer, "%d", count);
  _outtext(buffer);
  _settextposition(3, 6);
  for(i = 0 ; i < max_raw && i < count ; i++) {
    sprintf(buffer, "%02x ", raw[i]);
    _outtext(buffer);    
  }  
}

void stats(int g)
{
  struct rccoord crd;
  int cl;  
  crd = _gettextposition();
  cl = _gettextcolor();

  window();
  switch(g) {
    case 0:
      generic();
      break;
    default:
      ;
  }

  _settextposition(crd.row, crd.col);
  _settextcolor(cl);
}

void update()
{
  outp(0x21, inp(0x21) | (1 << irq));
  if(1) stats(0);
  outp(0x21, inp(0x21) & ~(1 << irq));
}

void feed_raw(b)
{
  int i = 0;
  for(i = 1 ; i < max_raw && i < count ; i++) {
    raw[i - 1] = raw[i];
  }
  raw[count < max_raw ? count : max_raw - 1] = b;
  count++;
}

void feed(int b)
{
  feed_raw(b);
}

void _WCINTERRUPT our_handler()
{
  int m = 0;

  do {
    while(inp(port+5) & 1) feed(inp(port));
    outp(port + 3, inp(port + 3) & 0x7f);
    m = inp(port + 2);
  } while(m & 0xe == 4);

  if(thorough) {
    m = inp(port + 4);
    if(((m & 0xb) != 0xb) || (m & 0x30)) {
      if(verbosity >= 2) {
        if(~m & 1) printf("Restoring DTR (%xH)\n", port);
        if(~m & 2) printf("Restoring RTS (%xH)\n", port);
        if(~m & 8) printf("Restoring AUX2 (%xH)\n", port);
        if(m & 0x20) printf("Removing autoflow control (%xH)\n", port);
        if(m & 0x10) printf("Removing loopback mode (%xH)\n", port);
      }
      outp(port + 4, (m & 0xcf) | 0xb);
    }
  }

  outp(0x20, 0x20);

  if(org_handler)
    _chain_intr(org_handler);
}


void doneprog(void) {
  handler_t org = org_handler;

  if(org_col >= 0) {
    _settextcolor(org_col);
    _settextposition(org_pos.row, org_pos.col);
    _outtext("\n");
    org_col = -1;
  }

  if (!has_exited) {
    int m;
    if(verbosity > 0) printf("Exiting\n"); 

    has_exited=1;   
    org_handler = 0;

    if(org != 0) {        
      _dos_setvect(8 + irq, org);
    }

    if(org != _dos_getvect(8 + irq)) fprintf(stderr, "Error restoring ISR\n");

    m = inp(0x21);
    if (verbosity >= 2 && (org_mask & (1 << irq))) printf("Masking IRQ%d\n", irq);
    outp(0x21, m | (org_mask & (1 << irq)));

    outp(port + 3, inp(port+3) & 0x7f);
    if(~org_ier & 1) {
      if(verbosity >= 2) printf("Disabling UART interrupts (%xH)\n", port);
    }
    outp(port + 1, org_ier);
    if(((org_mcr & 0xb) != 0xb) || (org_mcr & 0x30)) {
      if(verbosity >= 2) {
        if(~org_mcr & 1) printf("Disabling DTR (%xH)\n", port);
        if(~org_mcr & 2) printf("Disabling RTS (%xH)\n", port);
        if(~org_mcr & 8) printf("Disabling AUX2 (%xH)\n", port);
        if(org_mcr & 0x20) printf("Enabling autoflow control (%xH)\n", port);
        if(org_mcr & 0x10) printf("Enabling loopback mode (%xH)\n", port);
      }
    }
    outp(port + 4, org_mcr);

    outp(port + 3, inp(port+3) | 0x80);
    if(org_dll != 0x60 || org_dlm != 0) {
      if(verbosity >= 2) printf("Restoring original port transmission rate (%xH)\n", port);
    }
    outp(port, org_dll);
    outp(port + 1, org_dlm);

    if(verbosity >= 2) printf("Restoring original port line control (%xH)\n", port);
    outp(port + 3, org_lcr);

    exit(0);
  }
}

static char buf[2048];

void handler(int s)
{
  signal(s,SIG_IGN);  

  if(org_col >= 0) {
    _settextcolor(org_col);
    _settextposition(org_pos.row, org_pos.col);
    _outtext("\n");
    org_col = -1;
  }

  strcpy(buf,
         s==SIGSEGV ? "Segmentation Violation" :
         s==SIGINT  ? "Interrupted by User" :
         s==SIGILL  ? "Illegal Instruction" :
         s==SIGFPE  ? "Floating Point Exception" :
         s==SIGTERM ? "Killed" : "Terminated by signal");

  fprintf(stderr, "%s\n", buf);

  doneprog();
}

void hello(int argc, char ** argv)
{
  char * progname = argv[0] + strlen(argv[0]);
  while(progname > argv[0]) {
    progname -= 1;
    if(*progname == '\\' || *progname == '/') {
      progname +=1 ; break;
    }
  }
  printf("%s %s\n", progname, VERSION);
}

void help()
{
  printf("Usage:\n");
  doneprog();
}

void summary(int i, int p, int v)
{
  if(v <= 0) return;

  printf("IRQ%d, IO%xH\n", i, p);
  if(v >= 2) printf("Verbosity: %d\n", v);
}

void main(int argc, char ** argv)
{
  int i = 4, p = 0x3f8, j = 0, v = 0, x = 0, b = 1200, d = 60;
  handler_t org = 0;

  hello(argc, argv);   
  for(j = 1 ; j < argc ; j++) {
    while(*argv[j] == '-' || *argv[j] == '/' || *argv[j] == ' ') argv[j] += 1;
    if(*argv[j] == 'i' || *argv[j] == 'I') i = atoi(argv[j]+1);
    if(*argv[j] == 'p' || *argv[j] == 'P') sscanf("%x", argv[j]+1, &p);
    if(*argv[j] == 'v' || *argv[j] == 'v') v = atoi(argv[j]+1);
    if(*argv[j] == '?' || *argv[j] == 'h' || *argv[j] == 'h') help();
    if(*argv[j] == 'x' || *argv[j] == 'X') x = 1;
    if(*argv[j] == 'b' || *argv[j] == 'B') b = atoi(argv[j]+1);
  }    
  summary(i, p, v);
  verbosity = v;
  irq = i;
  port = p;
  thorough = x;

  switch(b)
  {
    case 2400:
      d = 0x30;
      break;
    default:
      ;
  }


  org = _dos_getvect(8 + irq);
  if(org) {
    atexit(doneprog);
    signal(SIGSEGV, handler);
    signal(SIGTERM, handler);
    signal(SIGILL,  handler);
    signal(SIGFPE,  handler);
    signal(SIGILL,  handler);
    signal(SIGINT,  handler);  
    signal(SIGABRT, handler);

    _dos_setvect(8 + irq, our_handler);
    if(_dos_getvect(8 + irq) == our_handler) { 
      org_handler = org;

      org_lcr = inp(port + 3);
      outp(port + 3, org_lcr & 0x7f);

      org_ier = inp(port + 1);

      if(x) {
        outp(port + 1, 0);
      }

      if(x) {
        if(verbosity >= 2) { 
          printf("Purged LSR and MSR of port (%xH)\n", j, port);
        }
        inp(port+5);
        inp(port+6);    
      }

      org_mcr = inp(port + 4);
      if(((org_mcr & 0xb) != 0xb) || (org_mcr & 0x30)) {
        if(verbosity >= 2) {
          if(~org_mcr & 1) printf("Enabling DTR (%xH)\n", port);
          if(~org_mcr & 2) printf("Enabling RTS (%xH)\n", port);
          if(~org_mcr & 8) printf("Enabling AUX2 (%xH)\n", port);
          if(org_mcr & 0x20) printf("Disabling autoflow control (%xH)\n", port);
          if(org_mcr & 0x10) printf("Disabling loopback mode (%xH)\n", port);
        }
        outp(port + 4, (org_mcr & 0xcf) | 0xb);
      }

      outp(port + 3, inp(port+3) | 0x80);
      org_dll = inp(port);
      org_dlm = inp(port + 1);
      if(org_dll != d & 0xff || org_dlm != (d >> 8) & 0xff) {
        if(verbosity >= 2) printf("Switching port to %d baud (%xH)\n", b, port);
        outp(port, d&0xff);
        outp(port + 1, (d >> 8) & 0xff);
      }

      if(verbosity >= 2) { 
        printf("Setting port line control to 7b 1s no parity (%xH)\n", port);
      }
      outp(port + 3, 2);

      if(x) {
        if(verbosity >= 2) { 
          printf("Purged FIFO buffers for port (%xH)\n", port);
        }
        outp(port + 2, 0x6);
      }

      org_mask = inp(0x21);
      if(org_mask & (1 << irq)) {
        outp(0x21, org_mask & ~(1 << irq));
        if (verbosity >= 2) printf("Unmasked IRQ%d: %02x\n", irq, inp(0x21));
      }

      if(~org_ier & 1) {
        if(verbosity >= 2) printf("Enabling UART interrupts (%xH)\n", port);
      }
      outp(port + 1, org_ier | 0x1);

      for((x ? inp(port) : 0), j = 0 ; inp(port+5) & 1 ; j++) feed(inp(port));
      if(verbosity >= 2) { 
        printf("Read %d more bytes from the port (%xH)\n", j, port);
      }
      
      if(x && verbosity >= 2) { 
        printf("Confirming DTR as %s (%xH)\n", (inp(port + 4) & 1) ? "ON" : "OFF", port);
        printf("Confirming RTS as %s (%xH)\n", (inp(port + 4) & 2) ? "ON" : "OFF", port);
        printf("Confirming LCR/ISR state as %02x/%02x for (%xH)\n", inp(port + 3), inp(port + 1), port);
      }
    }
  }

  if(!org_handler) {
    fprintf(stderr, "Error installing ISR");
  } else {
    org_pos = _gettextposition();
    org_col = _gettextcolor();
    while(1) { 
      char buf[2] = {0 , 0};
      char *tmpl = "|/=\\";
      delay(99); 
      _settextposition(org_pos.row, org_pos.col);  
      _settextcolor(2);
      sprintf(buf, "%c", tmpl[j++%4]); 
      _outtext(buf);
      _settextcolor(org_col);
      update();
    }
  }
}
