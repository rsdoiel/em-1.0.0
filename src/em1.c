/*
 * Unix 6
 * Editor
 * Original code by Ken Thompson
 *
 * QMC mods Feb. 76, by  George Coulouris
 * mods are:
	prompts (suppress with '-p' flag)
	",%,&, to display a screen full of context
	'x' - as 's' but interactive
	'n' flag when appended to 's' or 'x' commands prints number of replacements

 * also mods by jrh 26 Feb 76
	% == current file name in ! commands
	!! == repeat the last ! command you executed
	-e flag == "elfic" mode :-
		no "w", "r\n" commands, auto w before q

    More mods by George March 76:

	'o' command for text input with local editing via control keys
	'b' to set a threshold for automatic line breaks in 'o' mode.
	'h' displays a screen full of help with editor commands
		(the help is in /usr/lib/emhelp)

    Port to modern unix (posix, c99) pierre.gaston@gmail.com 2012

bugs:
	should not use printf in substitute()
	(for space reasons).
 */

/* this file contains all of the code except that used in the 'o' command.
	that is in a second segment called em2.c */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>


/* screen dimensions */
#define LINES	18
#define LENGTH	80
#define SPLIT	'-'
#define PROMPT	'>'
#define CONFIRM	'.'
#define SCORE	"^"
#define FORM	014
/* #define	SIGHUP	1 */
/* #define	SIGINTR	2 */
/* #define	SIGQUIT	3 */
#define	FNSIZE	64
#define	LBSIZE	512
#define	ESIZE	128
#define	GBSIZE	256
#define	NBRA	5
/* #define	EOF	-1 */

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12

#define	STAR	01

#define	error  errfunc() /*goto errlab pgas: don't think you can goto a pointer label in modern C*/

#define	READ	0
#define	WRITE	1


#define UNIXBUFL 100

extern int margin;	/* used to set threshold in 'open' */

int	elfic	= 0;	/* true if "elfic" (-e) flag */
int firstime	= 1;	/* ugh - used to frigg initial "read" */
int	peekc;
int	lastc;
char	unixbuffer [UNIXBUFL];
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	rhsbuf[LBSIZE/2];
char	expbuf[ESIZE+4];
int	circfl;
int	*zero;
int	*dot;
int	*dol;
/* int	*endcore; replaced by malloc*/
/* int	*fendcore; */
int	*addr1;
int	*addr2;
char	genbuf[LBSIZE];
long	count;
char	*nextip;
char	*linebp;
int	ninbuf;
int	io;
int	pflag;
struct sigaction	onhup;
struct sigaction	onquit;
int	vflag	= 0;
int	xflag	= 0;	/*used in 'xchange' command */
int	listf;
int	col;
char	*globp;
int	tfile =	-1;
int	tline;
char	tfname[11];
char	*loc1;
char	*loc2;
char	*locs;
char	ibuff[512];
int	iblock	= -1;
char	obuff[512];
int	oblock =	-1;
int	ichanged;
int	nleft;

/*int	*errlab	= errfunc;  no need for this label */
char	TMPERR[] = "TMP";
int	names[26];
char	*braslist[NBRA];
char	*braelist[NBRA];
sigjmp_buf jmpbuf;
unsigned nlall = 128;

/*extern */
void op(size_t inglob);
void putch(char ch);

/* forward declaration */
int * address();
int advance(char* alp,char* aep);
int append(int (*f)(), int *a);
void  blkio(int b, char *buf,  ssize_t (*iofcn)(int, void *, size_t));
void breaks( char *p);
int cclass(char *aset, char ac, int af);
void commands(int prompt);
void compile(int aeof);
int compsub(); 
int confirmed();
void delete();
void donothing();
void dosub();
void errfunc();
int execute(int gf,int* addr);
void exfile();
void filename();
char* getblock(int atl,int  iof);
char getchr();
int getcopy(); 
int getfile();
char *em_getline(int tl);
int getsub(); 
int gettty();
void global(int k);
void init();
void move(int cflag);
void newline();
void nonzero();
void onintr(int );
char *place(char *asp, char *al1,char * al2);
void putchr(int ac);
void putd();
void putfile();
int putline();
void putstr(char *as);
void reverse(int *aa1, int *aa2);
void screensplit();
void setall();
void setdot();
void setnoaddr();
void substitute(size_t inglob);
void underline (char *line, char *l1,char * l2,char * score);
void callunix();

void main(int argc, char **argv)
{
	register char *p1, *p2;
        
        struct sigaction    act;
        act.sa_handler = SIG_IGN;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;

        sigaction(SIGQUIT, &act , &onquit);
        sigaction(SIGHUP, &act, &onhup);
        int lastc = 0;
        while (*(*argv+lastc+1) != '\0') lastc++;
	if(*(*argv+lastc) == 'm') vflag = 1;
	argv++;
	if (argc > 1 && **argv=='-') {
		p1 = *argv+1;
		while (*p1) {
			switch (*p1++) {
		case 'q':
                                sigaction(SIGHUP, (struct sigaction*)SIG_DFL, &onhup);
                                break;
		case 'e':
				elfic = 1;
				break;
		case 'p':
				vflag = 0;
				break;
		case 's':
				vflag = -1;
				break;
			}
		}
		if (!(*argv)[1])
			vflag = -1;
		argv++;
		argc--;
	}
	if (argc>1) {
		p1 = *argv;
		p2 = savedfile;
		while (*p2++ = *p1++);
		breaks(p1-3);
		globp = "r";
	}
	/* fendcore = sbrk(0); */
        zero = (int *)malloc(nlall*sizeof(int));
	if (vflag>0) putstr("Editor");
	init();
        
        struct sigaction	oldint;
        sigaction(SIGINT, NULL, &oldint);
	if (oldint.sa_handler != SIG_IGN) {
                  struct sigaction    act;
                  act.sa_handler = onintr;
                  sigemptyset(&act.sa_mask);
                  act.sa_flags = 0;
                  // onintr
                  sigaction(SIGINT, &act, NULL);
        }
     
	sigsetjmp(jmpbuf,1);
	commands(vflag);
	unlink(tfname);
}

void commands(int prompt)
{
	register int *a1, c;
	register char *p;
	char *p1,*p2;
	int fd, r, n;

	for (;;) {
	if (pflag) {
		pflag = 0;
		addr1 = addr2 = dot;
		goto print;
	}
	if (prompt>0 && globp == 0) putch(PROMPT);
	addr1 = 0;
	addr2 = 0;
	xflag = 0;
	do {
		addr1 = addr2;
		if ((a1 = address())==0) {
			c = getchr();
			break;
		}
		addr2 = a1;
		if ((c=getchr()) == ';') {
			c = ',';
			dot = a1;
		}
	} while (c==',');
	if (addr1==0)
		addr1 = addr2;
	if (c>= 'A' && c<= 'Z')
		c |= 040;
	switch(c) {

	case 'a':
		setdot();
		newline();
		append(gettty, addr2);
		continue;

	case 'b':
			if((c=peekc=getchr())== '+' || c =='-')
				peekc = 0;
                        else if(c != '\n') error;
			margin = c == '-' ? LBSIZE - 40 : LENGTH - 20;
			newline();
			continue;

	case 'c':
		setdot();
		newline();
		delete();
		append(gettty, addr1-1);
		continue;

	case 'd':
		setdot();
		newline();
		delete();
		continue;

	case 'e':
		if (elfic)
			error;
		setnoaddr();
		if ((peekc = getchr()) != ' ')
			error;
		savedfile[0] = 0;
		init();
		addr2 = zero;
		goto caseread;

	case 'f':
		if (elfic)
			error;
		setnoaddr();
		if ((c = getchr()) != '\n') {
			peekc = c;
			savedfile[0] = 0;
			filename();
		}
		putstr(savedfile);
		continue;

	case 'g':
		global(1);
		continue;

	case 'h':
		newline();
		if((fd = open(HELP_PATH,0))<0) {
			putstr(HELP_PATH " not found");
			continue;
		}
			while (n = read( fd, linebuf, 512))
				write(1, linebuf, n);
			close( fd);
			continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		append(gettty, addr2-1);
		continue;

	case 'k':
		if ((c = getchr()) < 'a' || c > 'z')
			error;
		newline();
		setdot();
		nonzero();
		names[c-'a'] = *addr2 | 01;
		continue;

	case 'm':
		move(0);
		continue;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		goto print;

	case 'l':
		listf++;
	case 'p':
		newline();
	print:
		setdot();
		nonzero();
		a1 = addr1;
		do
			putstr(em_getline(*a1++));
		while (a1 <= addr2);
		dot = addr2;
		listf = 0;
		continue;

 	case 'o':
		setdot();
		op((size_t)globp);
		continue;

	case 'q':
		setnoaddr();
		newline();
		if (elfic) {
			firstime = 1;
			goto writeout;
		}
	quitit:
		unlink(tfname);
		exit(0);

	case 'r':
	caseread:
		filename();
		if ((io = open(file, 0)) < 0) {
			lastc = '\n';
			error;
		}
		setall();
		ninbuf = 0;
		append(getfile, addr2);
		exfile();
		continue;

	case 'x':
		xflag = 1;
	case 's':
		setdot();
		nonzero();
		substitute((size_t)globp);
		xflag = 0;
		continue;

	case 't':
		move(1);
		continue;

	case 'v':
		global(0);
		continue;

	case 'w':
		if (elfic)
			error;
	writeout:
		setall();
		nonzero();
		if (elfic) {
			p1 = savedfile;
			if (*p1==0)
				error;
			p2 = file;
			while (*p2++ = *p1++);
		}
		else
			filename ();
		if ((io = creat(file, 0666)) < 0)
			error;
		putfile();
		exfile();
		if (elfic)
			goto quitit;
		continue;

	case '"':
		setdot();
		newline();
		nonzero();
		dot = addr1;
		if (dot == dol) error;
		addr1 = dot+1;
		addr2 = dot +LINES-1;
		addr2 = addr2>dol? dol: addr2;
	outlines:
		putchr(FORM);
		a1 = addr1-1;
		while (++a1 <= addr2) putstr(em_getline(*a1));
		dot = addr2;
		continue;

	case '&':
		setdot();
		newline();
		nonzero();
		dot = addr1;
		addr1 = dot - (LINES-2);
		addr2 = dot;
		addr1 = addr1>zero? addr1: zero+1;
		goto outlines;

	case '%':
		newline();
		setdot();
		nonzero();
		dot = addr1;
		addr1 = dot - (LINES/2 - 2);
		addr2 = dot + (LINES/2 - 2);
		addr1 = addr1>zero? addr1 : zero+1;
		addr2 = addr2>dol? dol : addr2;
		a1 = addr1 - 1;
		putchr(FORM);
		while(++a1 <= addr2) {
			if (a1 == dot) screensplit();
			putstr(em_getline(*a1));
			if (a1 == dot) screensplit();
		}
		continue;

          case '>':
                newline();
                 vflag = vflag>0? 0: vflag;
		siglongjmp(jmpbuf,1);

	case '<':
                 newline();
		 vflag = 1;
		siglongjmp(jmpbuf,1);

	case '=':
		setall();
		newline();
		count = (addr2-zero)&077777;
		putd();
		putchr('\n');
		continue;

	case '!':
		callunix();
		continue;

	case EOF:
          if(prompt == -2 || isatty(0) == 0) return;
		continue;

	}
	error;
	}
}

int * address()
{
	register int *a1, minus, c;
	int n, relerr;

	minus = 0;
	a1 = 0;
	for (;;) {
		c = getchr();
		if ('0'<=c && c<='9') {
			n = 0;
			do {
				n *= 10;
				n += c - '0';
			} while ((c = getchr())>='0' && c<='9');
			peekc = c;
			if (a1==0)
				a1 = zero;
			if (minus<0)
				n = -n;
			a1 += n;
			minus = 0;
			continue;
		}
		relerr = 0;
		if (a1 || minus)
			relerr++;
		switch(c) {
		case ' ':
		case '\t':
			continue;
	
		case '+':
			minus++;
			if (a1==0)
				a1 = dot;
			continue;

		case '-':
		case '^':
			minus--;
			if (a1==0)
				a1 = dot;
			continue;
	
		case '?':
		case '/':
			compile(c);
			a1 = dot;
			for (;;) {
				if (c=='/') {
					a1++;
					if (a1 > dol)
						a1 = zero;
				} else {
					a1--;
					if (a1 < zero)
						a1 = dol;
				}
				if (execute(0, a1))
					break;
				if (a1==dot)
					{putchr('?'); error;}
                                /* two '?'s for failed search */
			}
			break;
	
		case '$':
			a1 = dol;
			break;
	
		case '.':
			a1 = dot;
			break;

		case '\'':
			if ((c = getchr()) < 'a' || c > 'z')
				error;
			for (a1=zero; a1<=dol; a1++)
				if (names[c-'a'] == (*a1|01))
					break;
			break;
	
		default:
			peekc = c;
			if (a1==0)
				return(0);
			a1 += minus;
			if (a1<zero || a1>dol)
				error;
			return(a1);
		}
		if (relerr)
			error;
	}
}

void setdot()
{
	if (addr2 == 0)
		addr1 = addr2 = dot;
	if (addr1 > addr2)
		error;
}

void setall()
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

void setnoaddr()
{
	if (addr2)
		error;
}

void nonzero()
{
	if (addr1<=zero || addr2>dol)
		error;
}

void newline()
{
	register int c;

	if ((c = getchr()) == '\n')
		return;
	c = c >= 'A' && c <= 'Z' ? c + 32 : c;
	if (c=='p' || c=='l') {
		pflag++;
		if (c=='l')
			listf++;
		if (getchr() == '\n')
			return;
	}
	error;
}

void filename()
{
	register char *p1, *p2;
	register int c;

	count = 0;
	c = getchr();
	if (c=='\n' || c==EOF) {
		if (elfic && !firstime)
			error;
		else
			firstime = 0;
		p1 = savedfile;
		if (*p1==0)
			error;
		p2 = file;
		while (*p2++ = *p1++);
		return;
	}
	if (c!=' ')
		error;
	while ((c = getchr()) == ' ');
	if (c=='\n')
		error;
	p1 = file;
	do {
		*p1++ = c;
	} while ((c = getchr()) != '\n');
	*p1++ = 0;
	if (savedfile[0]==0) {
		p1 = savedfile;
		p2 = file;
		while (*p1++ = *p2++);
		breaks(p1 - 3);
	}
}

void breaks(char *p) 
{
	if(*p++ == '.')
		if(*p == 'r' || *p == 'n') margin = LENGTH -20;
}

void exfile()
{
	close(io);
	io = -1;
	if (vflag>=0) {
		putd();
		putchr('\n');
	}
}

void onintr(int signo)
{
	putchr('\n');
	lastc = '\n';
	error;
}

void errfunc()
{
	register int c;

	listf = 0;
	putstr("?");
	count = 0;
	lseek(0, 0,  SEEK_END);
	pflag = 0;
	if (globp)
          lastc = '\n';
	globp = 0;
	peekc = lastc;
	while ((c = getchr()) != '\n' && c != EOF);
	if (io > 0) {
		close(io);
		io = -1;
	}
	siglongjmp(jmpbuf,1);
}

char getchr()
{
	if (lastc=peekc) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = *globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	if (read(0, &lastc, 1) <= 0)
		return(lastc = EOF);
	lastc &= 0177;
	return(lastc);
}

int gettty()
{
       register int c;
       register char * gf;
       register char *p;

	p = linebuf;
	gf = globp;
	while ((c = getchr()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if ((c &= 0177) == 0)
			continue;
		*p++ = c;
		if (p >= &linebuf[LBSIZE-2])
			error;
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	return(0);
}

int getfile()
{
	register int c;
	register char *lp, *fp;

	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, LBSIZE)-1) < 0)
				return(EOF);
			fp = genbuf;
		}
		if (lp >= &linebuf[LBSIZE])
			error;
		if ((*lp++ = c = *fp++ & 0177) == 0) {
			lp--;
			continue;
		}
		count ++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

void  putfile()
{
	int *a1;
	register char *fp, *lp;
	register int nib;

	nib = 512;
	fp = genbuf;
	a1 = addr1;
	do {
		lp = em_getline(*a1++);
		for (;;) {
			if (--nib < 0) {
				write(io, genbuf, fp-genbuf);
				nib = 511;
				fp = genbuf;
			}
                     count++;
			if ((*fp++ = *lp++) == 0) {
				fp[-1] = '\n';
				break;
			}
		}
	} while (a1 <= addr2);
	write(io, genbuf, fp-genbuf);
}

int append(int (*f)(), int *a)
{
  register int *a1;
  register int *a2;
  register int *rdot;
  int nline, tl;

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
		if ((dol-zero)+1 >= nlall) {
			int *ozero = zero;
			nlall += 512;
			/* free((char *)zero); */
			if ((zero = (int *)realloc((char *)zero, nlall*sizeof(int)))==NULL) {
				lastc = '\n';
				zero = ozero;
				error;
			}
			dot += zero - ozero;
			dol += zero - ozero;
		}
		tl = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot)
			*--a2 = *--a1;
		*rdot = tl;
	}
	return(nline);
}

void callunix()
{
	register int  pid, rpid;
        struct sigaction   saveint;
	int retcode;
	char c,*lp,*fp;
	pid = 0;
	if ((c=getchr ()) != '!') {
		lp = unixbuffer;
		do {
			if (c != '%')
				*lp++ = c;
			else {
			pid = 1;
				fp = savedfile;
				while ((*lp++ = *fp++));
				lp--;
			}
			c = getchr();
		} while (c != '\n');
		*lp = '\0';
	}
	else { pid = 1;
		while (getchr () != '\n');}
	if(pid) {
		putchr('!');
		putstr(unixbuffer);
	}
	setnoaddr();
	if ((pid = fork()) == 0) {
                sigaction(SIGQUIT, &onquit, NULL);
                sigaction(SIGHUP, &onhup, NULL);
		execl ("/bin/sh", "sh", "-c", unixbuffer, (char *)0);
		exit(0100);
	}
        struct sigaction    act;
        act.sa_handler = SIG_IGN;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;

        sigaction(SIGINT, &act, &saveint);   
	while ((rpid = wait(&retcode)) != pid && rpid != -1);
        sigaction(SIGHUP, &saveint, NULL);
	putstr("!");
}

void delete()
{
	register int *a1, *a2, *a3;

	nonzero();
	a1 = addr1;
	a2 = addr2+1;
	a3 = dol;
	dol -= a2 - a1;
	do
		*a1++ = *a2++;
	while (a2 <= a3);
	a1 = addr1;
	if (a1 > dol)
		a1 = dol;
	dot = a1;
}

char *em_getline(int tl)
{
	register char *bp, *lp;
	register int nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~0377;
	while (*lp++ = *bp++)
		if (--nl == 0) {
			bp = getblock(tl=+0400, READ);
			nl = nleft;
		}
	return(linebuf);
}

int putline()
{
	register char *bp, *lp;
	register int nl;
	int tl;

	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~0377;
	while (*bp = *lp++) {
		if (*bp++ == '\n') {
			*--bp = 0;
			linebp = lp;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl += 0400, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char *getblock(int atl, int iof)
{
	/* extern read(), write(); */
	register int bno, off;
	
	bno = (atl>>8)&0377;
	off = (atl<<1)&0774;
	if (bno >= 255) {
		putstr(TMPERR);
		error;
	}
	nleft = 512 - off;
	if (bno==iblock) {
		ichanged |= iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged)
                  blkio(iblock, ibuff, (ssize_t (*)(int, void *, size_t))write);
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, read);
		return(ibuff+off);
	}
	if (oblock>=0)
		blkio(oblock, obuff, (ssize_t (*)(int, void *, size_t))write);
	oblock = bno;
	return(obuff+off);
}

void blkio(int b, char *buf,  ssize_t (*iofcn)(int,  void *, size_t))
{
	lseek(tfile, b*512, SEEK_SET);
	if ((*iofcn)(tfile, buf, 512) != 512) {
		putstr(TMPERR);
		error;
	}
}

void init()
{
	register char *p;
	register int pid;

	close(tfile);
	tline = 0;
	iblock = -1;
	oblock = -1;
	memcpy( &tfname[0], "/tmp/exxxxx",11);
	ichanged = 0;
	pid = getpid();
	for (p = &tfname[11]; p > &tfname[6];) {
		*--p = (pid&07) + '0';
		pid >>= 3;
	}
	close(creat(tfname, 0600));
	tfile = open(tfname, 2);
	/* brk(fendcore); */
	dot = dol = zero; /* = dol = fendcore; */
	/* endcore = fendcore - 2; */
}

void global(int k)
{
	register char *gp;
	register int c;
	register int *a1;
	char globuf[GBSIZE];

	if (globp)
		error;
	setall();
	nonzero();
	if ((c=getchr())=='\n')
		error;
	compile(c);
	gp = globuf;
	while ((c = getchr()) != '\n') {
		if (c==EOF)
			error;
		if (c=='\\') {
			c = getchr();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			error;
	}
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			*a1 |= 01;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands(-2);
			a1 = zero;
		}
	}
}

void substitute(size_t inglob)
{
	register int gsubf, *a1, nl;
	int nflag, nn, getsub();

	gsubf = compsub();
	nflag = gsubf > 1 ? 1 : 0;
	nn = 0;
	gsubf &= 01;
	gsubf |= xflag;
	for (a1 = addr1; a1 <= addr2; a1++) {
		if (execute(0, a1)==0)
			continue;
		inglob |= 01;
		if (confirmed()) { dosub(); nn++; }
		else donothing();
		if (gsubf) {
			while (*loc2) {
                          if (execute(1,(int *)0)==0)
					break;
		if(confirmed()) {  dosub(); nn++; }
		else donothing();
		}
	}
		*a1 = putline();
		nl = append(getsub, a1);
		a1 += nl;
		addr2 += nl;
	}
	if (inglob==0)
		{putchr('?'); error; }
			/* two queries distinguish failed match */
	/* should use putd() and count here */
	if (nflag) printf( " %d \n", nn);
}

void donothing() {
	char t1,t2;
			t1 = rhsbuf[0];
			t2 = rhsbuf[1];
			rhsbuf[0] = '&';
			rhsbuf[1] = 0;
			dosub();
			rhsbuf[0] = t1;
			rhsbuf[1] = t2;
}

int confirmed()
{
int ch;
	if(xflag) {
		putstr(linebuf);
		underline(linebuf, loc1, loc2, SCORE);
		ch = getchr();
		if ( ch != '\n') { while (getchr() != '\n');
				if ( ch != CONFIRM ) putstr("? '.' to confirm");
		}
		return (ch == CONFIRM ? 1: 0);
		}
	return 1;
}


void underline (char *line, char *l1,char * l2,char * score)
{
	char *ch, *ll; int i;
	register char *p;

	p = line;
	ch = " ";
	ll = l1;
	i = 2;
	while (i--) {
		while (*p && p < ll) {
			write (1, (*p == '\t' ? p : ch),1);
			p++;
		}
		ch = score;
		ll = l2;
	}
}

void screensplit()
{
	register int a;

        a = LENGTH;
        while(a--) putchr(SPLIT);
        putchr('\n');
}

int compsub()
{
	register int seof, c;
	register char *p;
	int gsubf;

	gsubf = 0;
	if ((seof = getchr()) == '\n')
		error;
	compile(seof);
	p = rhsbuf;
	for (;;) {
		c = getchr();
		if (c=='\\')
			c = getchr() | 0200;
		if (c=='\n')
			error;
		if (c==seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			error;
	}
	*p++ = 0;
	if(((peekc = getchr())| 040) == 'g') {
		peekc = 0;
		gsubf |= 1;
	}
	if (((peekc = getchr())| 040)  == 'n') {
		peekc = 0;
		gsubf |= 2;
	}
	newline();
	return(gsubf);
}

int getsub()
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while (*p1++ = *p2++);
	linebp = 0;
	return(0);
}

void dosub()
{
	register char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while (c = *rp++) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c<0 && (c &= 0177) >='1' && c < NBRA+'1') {
			sp = place(sp, braslist[c-'1'], braelist[c-'1']);
			continue;
		}
		*sp++ = c&0177;
		if (sp >= &genbuf[LBSIZE])
			error;
	}
	lp = loc2;
	loc2 = sp + (size_t)linebuf - (size_t)genbuf;
	while (*sp++ = *lp++)
		if (sp >= &genbuf[LBSIZE])
			error;
	lp = linebuf;
	sp = genbuf;
	while (*lp++ = *sp++);
}

char *place(char *asp, char *al1,char * al2)
{
	register char *sp, *l1, *l2;

	sp = asp;
	l1 = al1;
	l2 = al2;
	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE])
			error;
	}
	return(sp);
}

void move(int cflag)
{
	register int *adt, *ad1, *ad2;

	setdot();
	nonzero();
	if ((adt = address())==0)
		error;
	newline();
	ad1 = addr1;
	ad2 = addr2;
	if (cflag) {
		ad1 = dol;
		append(getcopy, ad1++);
		ad2 = dol;
	}
	ad2++;
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error;
}

void reverse(int *aa1, int *aa2)
{
	register int *a1, *a2, t;

	a1 = aa1;
	a2 = aa2;
	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

int getcopy()
{
	if (addr1 > addr2)
		return(EOF);
	em_getline(*addr1++);
	return(0);
}

void compile(int aeof)
{
	register int eof, c;
	register char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
	int nbra;
	int cclcnt;

	ep = expbuf;
	eof = aeof;
	bracketp = bracket;
	nbra = 0;
	if ((c = getchr()) == eof) {
		if (*ep==0)
			error;
		return;
	}
	circfl = 0;
	if (c=='^') {
		c = getchr();
		circfl++;
	}
	if (c=='*')
		goto cerror;
	peekc = c;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = getchr();
		if (c==eof) {
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchr())=='(') {
				if (nbra >= NBRA)
					goto cerror;
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					goto cerror;
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			*ep++ = CCHR;
			if (c=='\n')
				goto cerror;
			*ep++ = c;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			goto cerror;

		case '*':
			if (*lastep==CBRA || *lastep==CKET)
				error;
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc=getchr()) != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchr()) == '^') {
				c = getchr();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n')
					goto cerror;
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					goto cerror;
			} while ((c = getchr()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	error;
}

int execute(int gf, int *addr)
{
	register char *p1, *p2, c;

	if (gf) {
		if (circfl)
			return(0);
		p1 = linebuf;
		p2 = genbuf;
		while (*p1++ = *p2++);
		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = em_getline(*addr);
		locs = 0;
	}
	p2 = expbuf;
	if (circfl) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	/* regular algorithm */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

int advance(char* alp,char* aep)
{
	register char *lp, *ep, *curlp;
	char *nextep;

	lp = alp;
	ep = aep;
	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)));
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp==locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		error;
	}
}

int cclass(char *aset, char ac, int af)
{
	register char *set, c;
	register int n;

	set = aset;
	if ((c = ac) == 0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

void putd()
{
	register int r;

        r = count %10;
        count /= 10;
	if (count)
		putd();
	putchr(r + '0');
}

void putstr(char *as)
{
	register char *sp;

	sp = as;
	col = 0;
	while (*sp)
		putchr(*sp++);
	putchr('\n');
}

char	line[70];
char	*linp	= line;

void putchr(int ac)
{
	register char *lp;
	register int c;

	lp = linp;
	c = ac;
	if (listf) {
		col++;
		if (col >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (c=='\t') {
			c = '>';
			goto esc;
		}
		if (c=='\b') {
			c = '<';
		esc:
			*lp++ = '-';
			*lp++ = '\b';
			*lp++ = c;
			goto out;
		}
		if (c<' ' && c!= '\n') {
			*lp++ = '\\';
			*lp++ = (c>>3)+'0';
			*lp++ = (c&07)+'0';
			col =+ 2;
			goto out;
		}
	}
	*lp++ = c;
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
}

/* /\* */
/*  * Get process ID routine if system call is unavailable. */
/* getpid() */
/* { */
/* 	register f; */
/* 	int b[1]; */

/* 	f = open("/dev/kmem", 0); */
/* 	if(f < 0) */
/* 		return(-1); */
/* 	seek(f, 0140074, 0); */
/* 	read(f, b, 2); */
/* 	seek(f, b[0]+8, 0); */
/* 	read(f, b, 2); */
/* 	close(f); */
/* 	return(b[0]); */
/* } */
/*  *\/ */
