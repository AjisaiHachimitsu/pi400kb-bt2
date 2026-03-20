#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION 25
#define TEXTSIZE 32768

int init_blue(char *fname);
int check_init(int flag);
int exitchar(void);
int connectdongle(int clicom);
int tryconn(int comport);
int sendcmd(int opcode,int *ndat,int ndatlen,
                    unsigned char *dat,int datlen);
int readreply(int wantopcode,int wantid,unsigned char *rdat,int rlen,int timeout);
void getints(unsigned char *buf,int *ndat,int ndatlen);
int readn(unsigned char *buf,int len);
int writen(unsigned char *dat,int datlen);
int ping(void);
int sendkeyx(int key);
void printn(char* txt, int len);
int findtext(char *s);
int savetxt(char *filename);

int sendpack(unsigned char* buf, int len);
int readpack(unsigned char* buf, int toms);
void printn(char* buf, int len);
int inithci(int comport);
int closehci(void);
void inputpin(char* prompt, char* sbuf);
int setkeymode(int setflag);
int readkey(void);
unsigned long long time_ms();
int checkfilename(char* funs, char* s);
int getdatfile(char *s);
void serverexit(int flag);
int packet_size(int size);
int dongtype(void);

struct gdat
  {
  int comport;
  int dongtype;  // 0=Pi4/PiZero  1=Pico
  int dongver;
  int runflag; // 0=start 1=bluetooth OK 
  HANDLE hCom;  // dongle COM port
  unsigned char dongdat[8192];
  char txt[TEXTSIZE];     // screen text
  unsigned int txtn;  // txt[] number of bytes
  };

struct gdat gparw;

int inithci(int comport)
  {
  int ret,cport;
  
  ret = 0;
  if(comport == 0)
    {
    printf("Enter COM port number ");
    scanf("%d",&cport);
    gparw.hCom = INVALID_HANDLE_VALUE;  
    gparw.txt[0] = 0;
    gparw.txtn = 0;
    ret = tryconn(cport);
    }
  else if(comport == -1)
    {
    for(cport = 1 ; cport <= 20 && ret == 0 ; ++cport)
      ret = tryconn(cport);
    }
  else if(comport > 0)
    ret = tryconn(comport);
    
  return(ret); 
  }

void serverexit(int flag)
  {
  return;
  }

int sendkeyx(int key)
  {
  int c;
  
  c = 0;
  if(kbhit() != 0)
    c = getch();
  return(c);
  }

#ifdef BTFPYTHON
void print(char *txt)
  {
  printn(txt,strlen(txt));
  }
#endif

void printn(char *txt,int len)
  {
  int n;
  unsigned int k;

  if((int)gparw.txtn > TEXTSIZE - len - 2000)
    {  // buffer full
    k = 4000;
    while(gparw.txt[k] != 10 && k < gparw.txtn)
      ++k;
    if(k == gparw.txtn)
      {
      gparw.txt[0] = 0;
      gparw.txtn = 0;
      }
    else
      {
      ++k;
      n = 0;
      while(k < gparw.txtn)
        {
        gparw.txt[n] = gparw.txt[k];
        ++n;
        ++k;
        }
      gparw.txt[n] = 0;
      gparw.txtn = n;  
      }
    }
  
  for(n = 0 ; n < len ; ++n)
    {
    if(txt[n] == 10 && gparw.txtn != 0 && gparw.txt[gparw.txtn-1] != 13)
      {  // ensure 0D 0A line end
      gparw.txt[gparw.txtn] = 13;
      ++gparw.txtn;
      }
    gparw.txt[gparw.txtn] = txt[n];
    ++gparw.txtn;
    }
  gparw.txt[gparw.txtn] = 0;
  printf("%s",gparw.txt);
  }


int savetxt(char *filename)
  {
  int ret;
  char txt[256];
  FILE *stream;

  ret = 0;
  stream = fopen(filename,"wb");
  if(stream == NULL)
    {
    print("Open file failed\n");
    return(0);
    }
  if(fwrite(gparw.txt,1,gparw.txtn,stream) == gparw.txtn)
    {
    ret = 1;
    sprintf(txt,"Screen text saved to %s\n",filename);
    print(txt);
    }
  else
    print("Failed to write screen text to file\n");
  fclose(stream);
  return(ret);
  }

int readreply(int wantopcode,int wantid,unsigned char *rdat,int rlen,int timeout)
  {
  int opcode,len,nread,retval,ib0,readflag;
  unsigned char head[8];
  unsigned char id;
  BOOL ret;
  unsigned long long tim0;

  if(gparw.hCom == INVALID_HANDLE_VALUE)
    return(-4);

  retval = -1;
  tim0 = time_ms();
  do
    {
    ret = ReadFile(gparw.hCom,head,1,&nread,NULL); 
    if(ret == 0)
      ret = 0;
    if(nread == 1)
      {
      ib0 = head[0];
      if(ib0 == 0xF8 || ib0 == 0xF7)
        {
        ret = readn(head+1,4);
        opcode = (int)(head[1]);
        id = head[2];
        len = (head[4] << 8) + head[3];
           
        readflag = 0;  // must read len
        if(ib0 == 0xF8 && opcode == wantopcode && id == wantid)
          {  // got reply
          if(len <= rlen)
            {
            retval = readn(rdat,len);  // buffer size OK
            readflag = 1;
            }
          else
            {
            print("Buffer too small in readreply\n");
            retval = 0;
            }
          }
        if(readflag == 0)
          nread = readn(gparw.dongdat,len);  // 8192 dump
        }
      // else not F7/F8
      }
    if(timeout > 0)
      {
      if((int)(time_ms() - tim0) > timeout)
        {
        retval = -2;
        print("Timed out waiting for reply from dongle\n");
        }
      }
    }
  while(retval == -1);  
  return(retval);
  }

void getints(unsigned char *buf,int *ndat,int ndatlen)
  {
  int n;
  long *lp;
  unsigned char *cp;

  cp = buf;
  for(n = 0 ; n < ndatlen ; ++n)
    {
    lp = (long*)cp;
    ndat[n] = (int)(*lp);
    cp += 4;
    }
  return;
  }

int readn(unsigned char *buf,int len)
  {
  int ntogo,nr,nread;

  if(len == 0)
    return(0);

  nread = 0;
  ntogo = len;
  do
    {
    if(ReadFile(gparw.hCom,buf+nread,ntogo,&nr,NULL) != 0)
      {
      if(nr > 0)
        {
        ntogo -= nr;
        nread += nr;
        }
      } 
    }
  while(ntogo > 0);    
  return(nread);
  }

int sendcmd(int opcode,int *ndat,int ndatlen,
                    unsigned char *dat,int datlen)
  {
  int n,dn,flag,nsend;
  unsigned char *cp,cmd[40];
  long *sp;
  static unsigned char id = 0;

  if(gparw.hCom == INVALID_HANDLE_VALUE)
    return(0);

  ++id;
  if(id == 0)
    id = 1;
 
  dn = 0;
  if(ndatlen > 0 && ndat != NULL)
    {
    cp = cmd+5;
    for(n = 0 ; n < ndatlen && n < 5 ; ++n)
      {
      sp = (long*)cp;
      *sp = (long)(ndat[n]);
      cp += 4;
      dn += 4;
      }
    }

  if((ndatlen*4)+datlen < 30 || dat == NULL)
    {
    flag = 0;  // one send
    if(datlen > 0 && dat != NULL)
      {
      for(n = 0 ; n < datlen ; ++n)
        {
        cmd[dn+5] = dat[n];
        ++dn;
        }
      }
    nsend = dn+5;
    }
  else
    {
    nsend = dn+5;  
    dn += datlen;
    flag = 1;
    }
     
  cmd[0] = 0xF7;
  cmd[1] = opcode;
  cmd[2] = id;
  cmd[3] = dn & 0xFF;
  cmd[4] = (dn >> 8) & 0xFF;

  writen(cmd,nsend);
  if(flag != 0)
    writen(dat,datlen);

  return(id); 
  }

int writen(unsigned char *dat,int datlen)
  {
  int wn,ntogo,nwrit;

  if(datlen == 0)
    return(0);

  ntogo = datlen;
  nwrit = 0;
  do
    {
    if(WriteFile(gparw.hCom,dat+nwrit,ntogo,&wn,NULL) != 0)
      {
      ntogo -= wn;
      nwrit += wn;
      } 
    }
  while(ntogo > 0);
  return(nwrit);  
  }

int ping()
  {
  int id,ret;
  unsigned long dat[4];
  char buf[64];

  static char* dtype[3] = {
  "PiZero/Pi4",
  "Pico",
  "Unknown" };

  static char* errs[7] = {
  "Bluetooth OK\n",
  "Dongle failed to open BTROTO socket\n",
  "Dongle failed to bind socket\n",
  "Dongle failed to start Bluetooth\n",
  "No reply from dongle - try re-starting it\n",
  "Unknown dongle type - Use latest Windows version\n",
  "Version mismatch - Use latest versions for Windows and dongle\n" };

  ret = 0;
  print("Ping dongle\n");
  id = sendcmd(1,NULL,0,NULL,0);
  if(readreply(1,id,(unsigned char*)dat,12,2000) == 12)
    {
    gparw.dongver = dat[0] & 0xFFFF;
    sprintf(buf,"Dongle replied OK. Version %d\n",gparw.dongver);
    print(buf);

    // If new Windows needs new dongle
    // if(VERSION > gparw.dongver)
    if(gparw.dongver == 21)  // sendpack changed
      {
      ret = 6;
      print(errs[ret]);
      return(ret); 
      }

    gparw.dongtype = (dat[0] >> 16) & 0xFFFF;
    if(gparw.dongtype < 0 || gparw.dongtype > 1)
      {  // valid dongtype = 0,1
      gparw.dongtype = 2;
      ret = 5;
      }
    sprintf(buf, "Dongle type = %s\n", dtype[gparw.dongtype]);
    print(buf);

    if(gparw.dongver != VERSION)
      print("BTferret and dongle versions different\n");
    
    if(ret == 0)
      {
      ret = (int)dat[1];
      if(ret < 0 || ret > 3)
        ret = 3;
      if((dat[2] & 0xFF) != 0)
        {
        sprintf(buf,"Failed %d setup calls\n",(dat[2] & 0xFF));
        print(buf);
        }
      }
    }
  else
    ret = 4;

  print(errs[ret]);
  return(ret);  // 0=OK
  }

int dongtype()
  {
  return(gparw.dongtype);
  }

int tryconn(int comport)
  {
  int ret,pret;
  char txt[64];

  ret = connectdongle(comport);
  if(ret != 0)
    {
    sprintf(txt,"COM%d open OK\n",comport);
    print(txt);
    pret = ping();
    if(pret == 0)
      {
      if(gparw.dongtype != 0)
        packet_size(220);
      gparw.runflag = 1;  // Bluetooth OK
      if(gparw.comport != comport)
        {
        gparw.comport = comport;
        }
      return(1);
      }
    else
      {
      print("Dongle setup failed or not running btfdongle\n");
      print("Try re-starting dongle\n");
      }
    CloseHandle(gparw.hCom); 
    gparw.hCom = INVALID_HANDLE_VALUE;
    }
  else
    {
    sprintf(txt,"COM%d open failed\n",comport);
    print(txt);
    }
  return(0);
  }

int connectdongle(int clicom)
  {
  DCB dcb;
  DWORD dwError;
  BOOL fSuccess;
  COMMTIMEOUTS cto;
  static char cliport[16] = {"\\\\.\\COM10"};

  if(clicom < 10)
    {
    cliport[7] = (char)(clicom + '0');
    cliport[8] = 0;
    }
  else
    {
    cliport[7] = (char)((clicom/10) + '0');
    cliport[8] = (char)((clicom%10) + '0');
    cliport[9] = 0;
    }

  gparw.hCom = CreateFile( cliport,
    GENERIC_READ | GENERIC_WRITE,
    0,    // comm devices must be opened w/exclusive-access 
    NULL, // no security attributes 
    OPEN_EXISTING, // comm devices must use OPEN_EXISTING 
    0,    // not overlapped I/O 
    NULL  // hTemplate must be NULL for comm devices
    );

 
  if(gparw.hCom == INVALID_HANDLE_VALUE) 
    {
    dwError = GetLastError();
    print("COM open error\n");
    return(0);
    }

  dcb.DCBlength = sizeof(dcb);
  fSuccess = GetCommState(gparw.hCom,(LPDCB)&dcb);
  if(!fSuccess) 
    {
    print("GetCommState error\n");
    CloseHandle(gparw.hCom);
    gparw.hCom = INVALID_HANDLE_VALUE;
    return(0);
    }

  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;

  fSuccess = SetCommState(gparw.hCom, &dcb);
  if(!fSuccess)  
   {
   print("SetCommState error\n");
   CloseHandle(gparw.hCom);
   gparw.hCom = INVALID_HANDLE_VALUE;
   return(0);
   }

  cto.ReadIntervalTimeout = 5; 
  cto.ReadTotalTimeoutMultiplier=5; 
  cto.ReadTotalTimeoutConstant=5; 
  cto.WriteTotalTimeoutMultiplier=5; 
  cto.WriteTotalTimeoutConstant=5; 

  fSuccess = SetCommTimeouts(gparw.hCom,&cto);
  if(!fSuccess)  
   {
   print("SetCommTimeouts error\n");
   }

  return(1);
  }


// Windows external
int sendpack(unsigned char* buf, int len)
  {
  int id;
  int ndat[1];
  unsigned char rdat[4];

  if(gparw.dongtype == 0 && gparw.dongver == 21)
    {
    ndat[0] = len;
    id = sendcmd(5, ndat, 1, buf, len);
    }
  else
    id = sendcmd(5,NULL,0, buf, len);

  readreply(5, id, rdat, 4, 0);
  
  return(rdat[0]);
  }  

int readpack(unsigned char* buf, int toms)
  {
  int id,len;
  int ndat[1];

  ndat[0] = toms;
  id = sendcmd(6, ndat,1,NULL,0);
  len = readreply(6, id, buf, 8192, 0);
  if(len < 0)
    len = 0;

  return(len);
  }


int closehci(void)
  {
  return(1);
  }
void inputpin(char* prom, char* sbuf)
  {
  printf("%s ",prom);
  scanf("%s",sbuf);  
  return;
  }
int setkeymode(int setflag)
  {
  return(0);
  }
int readkey(void)
  {
  return(sendkeyx(0));
  }
// in btlib.h
void scroll_back(void)
  {
  return;
  }
void scroll_forward(void)
  {
  return;
  }
unsigned long long time_ms()
  {
  return(GetTickCount64());
  }
void sleep_ms(int ms)
  {
  unsigned long long tim;

  tim = time_ms() + ms;
  while(time_ms() < tim)
    ;
  return;
  }

int checkfilename(char* funs, char* s)
  {
  int n, flag;

  flag = 0;
  for (n = 0; s[n] != 0 && flag == 0 ; ++n)
    {
    if (s[n] < 32 || s[n] > 122)
      flag = 1;
    }
  if (flag == 0)
    return(1);

  print("*** ERROR *** in ");
  print(funs);
  print(" - Cancelled - invalid character in file name\nMaybe single backslash control character in file name\n");
  print("For Windows use double backslash e.g. C:\\\\cat\\\\dog\\\\vole.txt\n");

  return(0);
  }

int getdatfile(char *dfile)
  {
  char *s,dirname[256];
  FILE *stream;

  s = getenv("USERPROFILE");
  if(s == NULL)
    {
    dfile[0] = 0;
    return(0);
    }
  strcpy(dfile,s);
  strcat(dfile,"\\AppData\\Local\\BTferret\\btferret.dat");

  stream = fopen(dfile, "rb");
  if (stream == NULL)
    {
    strcpy(dirname,s);
    strcat(dirname,"\\AppData\\Local\\BTferret");
    if(CreateDirectory(dirname,NULL) != 0)
      {
      stream = fopen(dfile,"wb");
      if(stream != NULL)
        {
        fputc(0,stream);
        fclose(stream);
        }
      }
    }
  else
    fclose(stream);
  return(1);
  }
