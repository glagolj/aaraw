/*
  #########################################################################
  *** DECOMPRESS AMBARELLA COMPRESSED RAW FILES ***


  COMPILE:
  gcc -Wall -march=native -O3 -lm -o amba_decompress.exe amba_decompress.c

  USAGE: 
  search for string USAGE or run: amba_decompress.exe -h

  COMPRESSION: 32 pixels are compressed into 27 bytes
    thus instead of 32*12bits=384bits algorithm uses only 27*8=216bits
    compression ratio is 216/384=56.25%, e.g. instead of 12bits,
    compression saves about 6.75bits. That sounds very drastic, but
    in reality nearby pixels will be very similar. However
    transitions between dark and bright areas will be more affected:
    look for artifacts there. 
    While cracking the algorithm I had an impression that compression
    program on ambarella is not always choosing optimal base levels,
    i.e. the employed compression algorithm could perform better.
    Since lines are padded to modulo 32, there are often additional
    pixels. As far as I can tell they are bogus, and are ignored here.
 
  PACKING OF BITS (those 27 bytes that contain 32 pixels):
     3 bits (common upper bits of 4 pixels)
     (6,6,6,6) bits for lower bits of 4 individual pixels
    Repeat group (3,6,6,6,6) eight times: 8*(3+6*4)==216bits=27bytes
    Final step is to reorder those 32 pixels (ordering is bayer aware, groups by colors).
    For details see function amba_decompress_27b()

  TESTED:
     Xiaomi Yi
     FF8SE

  Versions: 
     1.05 (2022/05/14)   small fixes for easier usage
     1.00 (2020/07/27)   first release
     0.90 (2019/09/14)   fix ~2 right edge pixels bug
 
  AUTHOR:
     https://github.com/glagolj
     https://glagolj.github.io/gg-blog/

  LICENCE: open source but crediting author would be very very nice.
  or if you must: "MIT or GPLv2 or GPLv3 or any later version"
  
  #########################################################################
 */

/* not all of these are needed, but I am lazy to fish them out */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sqrt() in guess_dim_from_size(), powf(), logf() */
#include <math.h>

/* C99 stdint.h --> uint64_t */
#include <stdint.h>

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/* 
   !!!!!!!!! THIS IS OLD FUNCTION, left here for documentation purposes 
*/
/* 
   WORKHOSE: decompress 27 bytes from the input stream and return 32 uint16 pixels
      q -> input 27 bytes, 
      r -> output 32 unsigned shorts
      user should allocate r and q (q must have q at least 28 bytes, even if only 27 are read)
*/
void amba_decompress_27b_old(unsigned char*q,unsigned short*r){

  /* order of 32 pixels: alternating groups of 4 (bayer aware: groups by colors) */
  static const int iord[]={0,2,4,6,      1,3,5,7,
			   8,10,12,14,   9,11,13,15,
			   16,18,20,22,  17,19,21,23,
			   24,26,28,30,  25,27,29,31}; 
  /* 
     one can generate iord[k] with:
         (k/8)*8 + ((k/4) & 1) + (k & 3)*2 
  */

  /* unsigned int u2pow[] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768}; */

  unsigned int k0=0,j0=0,i1,i2,bb,ee;
  
  /*
    int i;
    printf("Q=[");
    for(i=0 ; i<27 ; i++)printf("%02x,",(int)(q[i]));
    printf("]\n");
  */

  for( i1=0 ; i1<8 ; i1++ ){
    /* printf("i1=%d      j0=%d k0=%02d #b=%02d\n",(int)i1,(int)j0,(int)k0,(int)(8*k0+j0)); */
    bb = q[k0] + q[k0+1]*256u;
    ee = (bb >> j0) & 7u;


    j0 += 3;  /* we read 3 bits (for multiplication factor) */
    if( j0 > 8 ){
      j0 -= 8; 
      k0++; /* new byte */
    }

    for( i2=0 ; i2<4 ; i2++ ){
      /* printf("i2=%d i=%02d j0=%d k0=%02d #b=%02d\n",(int)i1,(int)(i1*4+i2),(int)j0,(int)k0,(int)(8*k0+j0)); */
      bb = q[k0] + q[k0+1]*256u;

      r[iord[i1*4+i2]] = ( (bb >> j0) & 63u ) << (ee+1);
      /* 
	 maybe one could improve precision here by guessing the rounding of the compression algorithm
	 and adding here (1<<ee) --> but this would need testing --> don't do it for the moment
       */

      j0 += 6;  /* we read 6 bits */
      if( j0 > 8 ){
	j0 -= 8; 
	k0++; /* new byte */
      }
    }
  }

  /*
    printf("R=[");
    for(i=0 ; i<32 ; i++)printf("%d,",(int)(r[i]));
    printf("]\n");
  */
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/*
  function amba_decompress_27b(): 
      decompress 27 bytes from the input stream and return 32 uint16 pixels
      q -> input 27 bytes, 
      r -> output 32 unsigned shorts
      user should allocate r and q (q must have at least 28 bytes, even if only 27 are read)

  FUNCTION amba_decompress_27b() works on both little and big endians
*/
/* TOTALY UNWIND 27bytes -> 32 pixels by hand! */
/*------------------------------*/
/* Do Ambarella compressed line. Each block is 27 input bytes and makes 32 output pixels.
   So total will be: input=27*nblocks bytes , output=32*nblocks pixels (where each pixel is 2 bytes)
   The reason for complications inside the function is to avoid branching and 
   examining nbits to load or not bucket bb.
   Interweaving loading bytes from q[] and outputting r[] is to keep all in 32 but bucket across 4 times loop (and withouth branches).
 */


#define amba_decompress_27b(x,y)     amba_decompress_27b_line((unsigned char*)x,(uint16_t*)y,1)
/* #define amba_decompress_27b(x,y)     amba_decompress_27b_old((unsigned char*)x,(uint16_t*)y) */

void amba_decompress_27b_line(unsigned char*q,uint16_t*r,unsigned nblocks){
  unsigned cnt,ee,bb;
  int      nbits;
  nblocks++;
  while( --nblocks ){
    bb     = q[0] | (q[1]<<8) | (q[2]<<16); /* 1st 3 bytes of 27byte block */
    q     += 3; /* 4 times loop add each time 6 bytes: total=3+4*6=27 */

    nbits  = 24 - (8 - 6 - 6); /* == 28 */
    /* nbits will show 8 less... --> to avoid another nbits += 8 after bb = bb | ... 
       This initial value 28 is such that the 1st "nbits += 8 - 6 - 6" evaluates to 24 in the 1st pass.
     */

    cnt    = 5;
    while(--cnt){
      /* HERE AT THE TOP OF THE LOOP:
	 bb contains ==> 1st pass: 24bits, 2nd: 18 bits, 3rd: 12bits, 4th: 06bits */

      bb  |= (unsigned)(*q++) << (nbits += 8 - 6 - 6);                     /* load 8bits */
      /* bb contains ==> 1st pass: 32bits, 2nd: 26 bits, 3rd: 20bits, 4th: 14bits */

      ee   = (  bb        &  7u ) + 1;   /* get 3 bits */
      r[0] = ( (bb >>  3) & 63u ) << ee; /* get 6 bits */
      /* 4th pass remains: 5bits */

      bb   = (bb >> (3+6)) | ( (unsigned)(*q++) << (nbits += 8 - 3 - 6) ); /* load 8bits */
      /* bb contains ==> 1st pass: 31bits, 2nd: 25 bits, 3rd: 19bits, 4th: 13bits */
      
      r[2] = (  bb        & 63u ) << ee; /* get 6 bits */
      r[4] = ( (bb >>  6) & 63u ) << ee; /* get 6 bits */
      /* 4th pass remains: 1bits */
      
      bb   = (bb >> (6+6)) | ( (unsigned)(*q++) << (nbits += 8 - 6 - 6) ); /* load 8bits */
      /* bb contains ==> 1st pass: 27bits, 2nd: 21 bits, 3rd: 15bits, 4th: 09bits */
      
      r[6] = (  bb        & 63u ) << ee; /* get 6 bits */
      
      ee   = ( (bb >>  6) &  7u ) + 1;   /* get 3 bits */
      /* 4th pass remains: 0bits */
      
      bb   = (bb >> (6+3)) | ( (unsigned)(*q++) << (nbits += 8 - 6 - 3) ); /* load 8bits */
      /* bb contains ==> 1st pass: 26bits, 2nd: 20 bits, 3rd: 14bits, 4th: 08bits */
      
      r[1] = (  bb        & 63u ) << ee; /* get 6 bits */
      /* 4th pass remains: 2bits */
      
      bb   = (bb >>  6   ) | ( (unsigned)(*q++) << (nbits += 8 - 6  ) ); /* load 8bits */
      /* bb contains ==> 1st pass: 28bits, 2nd: 22 bits, 3rd: 16bits, 4th: 10bits */
      
      r[3] = (  bb        & 63u ) << ee; /* get 6 bits */
      /* 4th pass remains: 4bits */
      
      bb   = (bb >>  6   ) | ( (unsigned)(*q++) << (nbits += 8 - 6  ) ); /* load 8bits */
      /* bb contains ==> 1st pass: 30bits, 2nd: 24 bits, 3rd: 18bits, 4th: 12bits */
      
      r[5] = (  bb        & 63u ) << ee; /* get 6 bits */
      r[7] = ( (bb >>  6) & 63u ) << ee; /* get 6 bits */
      /* 4th pass remains: 0bits */
      
      r   += 8; /* we did 8 pixels! */

      bb >>= 6 + 6; /* top of the loop expects bucket bb to be properly shifted */
      /* bb contains ==> 1st pass: 18bits, 2nd: 12 bits, 3rd: 06bits, 4th: 00bits */
    }
  }
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*
  xmod=16 and ymod=8 seems safe choice (from bunch of raw2nef.ini)
  aspect_r=1.32 is good starting point
  this can solve also amba compressed files
 */
int guess_dim_from_size  /* return 0 if OK, 1 if error! */
(
 unsigned size, unsigned xmod, unsigned ymod, float aspect_r, /* input */
 int*nx,int*ny
 )
{
  unsigned s=size/2,ax,ay;
  if( size & 1 ) return(1);
  if( (s/(xmod*ymod))*xmod*ymod != s )return(1);
  s /= xmod*ymod;
  ay = sqrtf(size/2.0f/aspect_r)/(float)(ymod) + 2;  /* ay=FLOOR(ay+2) */
  while( (s/ay)*ay != s ) ay--;  /* search down, to increasing aspect ratios */
  ax  = s / ay;
  *nx = ax*xmod;
  *ny = ay*ymod;
  return( ax > 1 && ay > 1 && (unsigned)(*nx * *ny * 2) == size ? 0 : 1 ); /* return 0 on success! */
}
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*  driver for above guess_dim_from_size() function; returns number of solutions (0==no solution!) */
int auto_detect_dim(unsigned size,int*nx,int*ny,int is_ambacmpr){ /* pitch will be: pitch==size/ny */
  int i,tmp,n=0; /* n is number of solutions */
  int x[4],y[4]; /* solutions */

  *nx = *ny = 0; /* no solution so far */

  /* try various aspects, like 4/3, or 16/9 or 1/1; also to report other possibilities */
  if( !guess_dim_from_size(size,16,8,1.32f*(is_ambacmpr?27.0f/64.0f:1.0f),x+n,y+n) ) n++;
  
  if( !guess_dim_from_size(size,16,8,1.75f*(is_ambacmpr?27.0f/64.0f:1.0f),x+n,y+n) ) if(n) if( x[n]!=x[n-1] || y[n]!=y[n-1] ) n++;

  if( !guess_dim_from_size(size,16,8,1.90f*(is_ambacmpr?27.0f/64.0f:1.0f),x+n,y+n) ) if(n) if( x[n]!=x[n-1] || y[n]!=y[n-1] ) n++;

  if( !guess_dim_from_size(size,16,8,1.00f*(is_ambacmpr?27.0f/64.0f:1.0f),x+n,y+n) ) if(n) if( x[n]!=x[0] || y[n]!=y[0] ) n++;
  
  if( !n ){
    printf("AutoDim: size=%u ==> no solutions for dimension found!\n",size);
    return(0); /* NO SOLUTIONS */
  }

  if(is_ambacmpr)for( i=0 ; i<n ; i++ ) x[i] = ( (2*x[i])/27 ) * 32;  /* compression factor is 27/64; output X is 32 mod */

  if( n>1 ) if( fabsf( x[0]/(float)(y[0]) - 4.0f/3.0f ) > fabsf( x[n-1]/(float)(y[n-1]) - 4.0f/3.0f ) ){
      tmp    = x[0];
      x[0]   = x[n-1]; /* solution #n-1 is closer to 4/3 aspect ratio; swap it! */
      x[n-1] = tmp;
      tmp    = y[0];
      y[0]   = y[n-1];
      y[n-1] = tmp;
    }

  *nx = x[0]; /* copy the solution */
  *ny = y[0]; 

  printf("AutoDim: size=%u #solutions=%i\n",size,n);
  for( i=0 ; i<n ; i++ )printf("  aspect=%.3f: -size %d %d -pitch %d\n",x[i]/(float)(y[i]),x[i],y[i],size/y[i]);

  return(n); /* number of solution */
}
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
int main(int in_argc, char **in_argv) {
  char  **argv = in_argv; /* manipulation with the input args */
  int     argc = in_argc;
  char*in_out_name=NULL;
  int  flaghelp        = in_argc <  2; /* argc=1 with no arguments */
  int  flag_parse_args = in_argc >= 2;
  int  i_arg,in_nx=0,in_ny=0,in_pitch=0;

  /* -------------------------------------------------------------------- */
  /* ----------- parse the arguments for additional settings ------------ */
  /* -------------------------------------------------------------------- */
  while( flag_parse_args && argc >= 2 && !flaghelp ){
    if( argc > 3 && strcmp("-o",argv[1]) == 0 ){
      in_out_name = argv[2];
      argc -= 2; /* correct the input for the next step */
      argv += 2; 
    }
    else if( argc > 4 && strcmp("-size",argv[1]) == 0 ){
      in_nx  = strtoul(argv[2], NULL, 0);
      in_ny  = strtoul(argv[3], NULL, 0);
      argc -= 3; /* correct the input for the next step */
      argv += 3; 
    }
    else if( argc > 3 && strcmp("-pitch",argv[1]) == 0 ){
      in_pitch  = strtoul(argv[2], NULL, 0);
      argc -= 2; /* correct the input for the next step */
      argv += 2; 
    }
    else if( strcmp("-h",argv[1]) == 0 || strcmp("-help",argv[1]) == 0 ){
      flaghelp = 1;
      argc--;/* correct the input for the next step */
      argv++;
    }
    else if( argv[1][0] == '-' ){
      printf("Unrecognized option '%s'!!!\n",argv[1]);
      flaghelp = 1;
      argc--;/* correct the input for the next step */
      argv++;
    }
    else flag_parse_args = 0;  /* argument not recognized stop parsing, even if argc>2 */
  }
  /* -------------------------------------------------------------------- */
  /* -------------------------------------------------------------------- */
  /* -------------------------------------------------------------------- */

  /* -------------------------------------------------------------------- */
  /* ------------- USAGE ------------------------------------------------ */
  /* -------------------------------------------------------------------- */
  if( argc < 2 || flaghelp ){
    printf("DECOMPRESS AMBARELLA COMPRESSED RAW FILES\n");
    printf("USAGE:\n  %s [-o FILE_OUT.RAW] [-size WIDTH HEIGHT] [-pitch PITCH] FILE_IN.RAW ...\n", in_argv[0]);
    printf("Simplest case: try to guess parameters from file size\n  %s FILE1.RAW FILE2.RAW ...\n", in_argv[0]);
    printf("Normal case: provide width and height\n  %s -size 3840 2880 FILE_IN.RAW\n", in_argv[0]);
    printf("Hard case: provide width and height and pitch (if file truncated or odd)\n  %s -size 3840 2880 -pitch 3264 FILE_IN.RAW\n", in_argv[0]);
    return(1);
  }
  /* -------------------------------------------------------------------- */
  /* -------------------------------------------------------------------- */
  /* -------------------------------------------------------------------- */

  if( argc>2 && in_out_name ){
    printf("ERROR: can't use -o option with multiple input files.\n");
    return 1;
  }

  for( i_arg=1 ; i_arg<argc ; i_arg++ ){
    char           *filename = argv[i_arg];
    char           *out_name = in_out_name;
    int             nx       = in_nx; /* from input arguments */
    int             ny       = in_ny;
    int             pitch    = in_pitch;
    unsigned char  *d;       /* input buffer */
    FILE           *fin,*fout;
    int             n,pitch_c,llen,nxm,iy;
    int             auto_nx=0,auto_ny=0;


    if( out_name == NULL ){ /* auto naming */
#define MAX_NAME (2048)
      char *p,out2[MAX_NAME];
      strncpy( out2, filename, MAX_NAME-16 );
      out2[MAX_NAME-16] = 0; /* just in case */
      for( p=out2 ; *p ; p++ );
      if( p-out2 > 4 )
	if( p[-4]=='.' && (p[-3]=='r' || p[-3]=='R') && (p[-2]=='a' || p[-2]=='A') && (p[-1]=='w' || p[-1]=='W') ) p-=4;
      *p++ = '_';
      *p++ = 'u';
      *p++ = 'n';
      *p++ = 'c';
      *p++ = '.';
      *p++ = 'r';
      *p++ = 'a';
      *p++ = 'w';
      *p++ = 0;
      out_name = out2;
    }


    fin = fopen(filename, "rb");
    if (!fin) {
      printf("ERROR: opening the input file '%s'.\n",filename);
      return 1;
    }

    fseek(fin, 0, SEEK_END);
    n = (int) ftell(fin);
    fseek(fin, 0, SEEK_SET);

    printf("**** input='%s' size=%d --> output='%s'\n",filename,n,out_name);
    
    /* try to guess width nx and height ny from the size of the input file */
    auto_detect_dim(n,&auto_nx,&auto_ny,1);

    if( nx <= 0 || ny <= 0 ){
      /* these are hardcoded sizes: not needed as auto_detect_dim() is guessing correctly! */
      if( n == 13492224 ){
	nx = 4608; /* Xiaomi Yi raw compressed file defaults */
	ny = 3456;
      }
      else if( n == 9400320 ){
	nx = 3840; /* FF8SE (A12 camera) */
	ny = 2880;
      }
      else if( n == 10176000 ){
	nx = 4000; /* FF8SE (A12 camera) */
	ny = 3000;
      }
      else if( n == 10368000 ){
	nx = 4096; /* FF8SE (A12 camera) */
	ny = 3000;
      }
    }
    if( (nx <= 0 || ny <= 0) && auto_nx>0 && auto_ny>0 ){
      nx = auto_nx;
      ny = auto_ny;
    }
    if( nx <= 0 || ny <= 0 ){
      printf("ERROR: please give '-size WIDTH HEIGHT' for the image. Could not guess from the file size!\n");
      exit(1); 
    }
    if( ny & 1 ){
      printf("ERROR: HEIGHT should be even!\n");
      exit(1); 
    }

    if( n == nx*ny*2 ){
      printf("WARNING: size of the input file == 2*nx*ny ---> are you sure that the file is compressed at all?????\n");
    }

    nxm  = (nx+31)/32; /* pad to 32 pixels width */

    /* pitch = #bytes per line */
    pitch_c = n/ny;    /* calculate: this should be suficient */
    if( pitch <= 0 ) pitch = pitch_c;
    if( pitch*ny != n ){
      printf("WARNING: The input file size should be == pitch*height!\n");
      printf(" pich_calculated=%d pich_user=%d\n",pitch_c,pitch);
    }
    if( pitch <= 0 || nxm <= 0 ){
      printf("ERROR: something is wrong with pitch or width or heigth!\n");
      return 1;
    }
    
    printf("using nx=%d ny=%d pitch=%d (aspect=%.3f blocks=%d extra=%d)\n",nx,ny,pitch,nx/(float)ny,nxm,pitch - nxm*27);
    fflush(stdout);
    
    llen = pitch > pitch_c ? pitch : pitch_c;
    llen = llen > nxm*27 ? llen : nxm*27;
    d = (unsigned char*) malloc(llen + 64); /* need few extra bytes, but lets overdo... */
    if(!d){
      printf("ERROR: allocating memory %d.\n",llen);
      return 1;
    }
    memset(d,0,llen+1); /* just in case for repeatable runs if truncation etc... */
    
    fout = fopen(out_name, "wb");
    if (!fout) {
      printf("ERROR: opening the output file '%s'.\n",out_name);
      return 1;
    }
    
    for(iy=0 ; iy < ny ; iy++){
      int ix,kk = pitch;
      if( (iy+1)*pitch > n ){ /* weird, file is maybe truncated */
	memset(d,0,llen+1); /* just in case for repeatable runs if truncation etc... */
	kk = n - iy*pitch;
	printf("WARNING iy=%d truncation kk=%d\n",iy,kk);
      }
      if( kk > 0 ) fread(d, 1, kk, fin); /* read whole line */
      
      for( ix=0 ; ix<nxm ; ix++ ){
	unsigned short  r[32];   /* output buffer */
	int ip,np = 32;
	amba_decompress_27b(d + ix*27,r); /* decompress 27 bytes and return 32 pixels */
	if( (ix+1)*32 > nx ) np = nx - ix*32;
	/* if( iy == 0 )printf("%d ",np); */
	
	for( ip=0 ; ip<np && ip<32 ; ip++ ){
	  fputc(  r[ip] & 255u      , fout ); /* so that this works both on small and big endians */
	  fputc( (r[ip] >> 8) & 255u, fout );
	}
      }
    }
    
    fclose(fout);
    fclose(fin);
    free(d);
  } /* end of for( i_arg=1 ; i_arg<argc ; i_arg++ ){} */

  return 0;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
