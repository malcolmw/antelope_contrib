/**********************************************************************
 *
 * 
 *  extrd/main.c 
 *  
 *  Program extract portion of the data form data file in css format.
 *  Program write in the current directory modified wfdisc file
 *  make directory with start time and write there portion of the data.
 *
 *
 *********************************************************************/
#include <signal.h>
#include "extrd.h"

extern void sig_hdlr();
int Seq;
char in_dbase[132];
 

main(argc, argv)
int argc;
char *argv[];

{

 Dbptr dbsub,tmpdb, wfddb;
 Tbl  *sort_sta_ch_tm;
 FILE *fd;
 double stime, etime;
 struct stat buf;
 char *out_dbase;
 char odir[132], exten[132];
 char path[132], name[132];
 char wfd_name[512], *fname;
 char *str;
 char *out_dir = NULL;
 char newdir[132], *tmpname;
 char rec[512];
 int nrec, oldsize, record_size, i, j;
 int wfdrec;
 char subset_condition[80];
 

    if( (out_dbase = (char *) malloc(132) ) == NULL )  {
       perror("extrd/main(): malloc");
       exit(1);
    }

    if( (fname = (char *) malloc(512) ) == NULL )  {
       perror("extrd/main(): malloc");
       exit(1);
    }

    if( (str = (char *) malloc(512) ) == NULL )  {
       perror("extrd/main(): malloc");
       exit(1);
    }



/*  Get input parameters from comand line and initialized its */

    if(argc < 4)  {
        fprintf(stderr,"\nusage:  %s dbname | wfd_dir | \"wfd_fls\" stime etime [out_dir]\n\n", argv[0]);
        fprintf(stderr,"  Arguments:\n\n");
        fprintf(stderr,"dbname    => DB name \n\n");
        fprintf(stderr,"wfd_dir   => directory with wfdisc files\n\n");
        fprintf(stderr,"wfd_fls   => Wfdisc files names. Quotes are required!\n\n");
        fprintf(stderr,"stime     => start time of the data subset\n\n");
        fprintf(stderr,"etime     => end time of the data subset\n\n");
        fprintf(stderr,"\nout_dir  => output directory. Current directory by default.\n");
        exit(1);
   }
   if(argc == 5)  {
       out_dir = argv[4];
       if(!create_dir(out_dir))   {
          die( 1, "Can't create an output directory: %s\n", out_dir); 
       }  
   } 
   if(strncmp(argv[1], "./", strlen("./")) == 0  ||  
      (strncmp(argv[1], ".", strlen(".")) == 0 && 
       strncmp(argv[1], "..", strlen("..")) != 0 ) )  {
      die(0, "extrd: Do not run extrd in the directory with original data.\n");
   }

    elog_init (argc, argv) ;
    elog_notify (0, "%s v2.0\n", argv[0] ) ;
    Program_Name = argv[0];

    signal(SIGINT, sig_hdlr );
    signal(SIGBUS, sig_hdlr );
    signal(SIGSEGV, sig_hdlr );

   stime = str2epoch(argv[2]);
   etime = str2epoch(argv[3]);
   
   tmpname = mktemp(strdup("extrdXXXXXX") );
   strcat( tmpname, ".wfdisc");
   sprintf( in_dbase, "%s\0",  tmpname );  
   
   if(stat(argv[1], &buf) == 0) {
     if(S_ISDIR(buf.st_mode) != 0)  {
        strcpy(path, argv[1]);
        sprintf(str, "ls %s/*.wfdisc > .tmp \0",argv[1]);
     }  else if(S_ISREG(buf.st_mode) != 0)  {

         namefrpath( argv[1], name);
         fexten(name, exten);
         if( strlen(exten) <= 0)  {
             get_wfd_name(argv[1], wfd_name);
                 sprintf(str, "ls %s > .tmp \0",wfd_name);
         } else if( strncmp(exten, "wfdisc", strlen("wfdisc")) == 0)  {
            sprintf(str, "ls %s > .tmp\0",argv[1]);
         }
     }
   } else  if(ENOENT) {
       fexten(argv[1], exten);
       if( strlen(exten) > 0 && strncmp(exten,"wfdisc", strlen("wfdisc") ) == 0)  {
           pathfrname(argv[1], path);
           sprintf(str, "ls %s > .tmp \0", argv[1]);
       }  else if(strlen(exten) <= 0)  {
           sprintf(wfd_name,"%s.wfdisc\0", argv[1]);
           if(stat(wfd_name, &buf) == 0) { 
               pathfrname(wfd_name, path);
               sprintf(str, "ls %s > .tmp\0",wfd_name );
           } else {
               die( 0, "Can't find wfdisc file.\n");
           }
       } else { 
	  die( 0, "Can't find wfdisc file.\n"); 
       } 
   } else { 
       die( 1, "Can't stat!\n");
   }
   system(str); 

   /*  Get file name from list file  */
 
   if ((fd = fopen(".tmp","r") ) == NULL) {
         unlink( ".tmp" ); 
         die( 1,"Can't open file .tmp \n");
    } 
    if (dbopen_database (in_dbase, "r+", &db) < 0)  {
         unlink( ".tmp" ); 
         die (0, "Can't open database %s\n", in_dbase);
    } 
    db = dblookup (db, 0, "wfdisc", 0, 0);
    if (db.table < 0)  {
         unlink( ".tmp" ); 
        die (0, "Can't open wfdisc table '%s'\n", in_dbase );
    }

    while((fname = fgets(fname, 132,fd)) != NULL)  {
      for(i = strlen(fname); i >= 0; i--) {
         if( fname[i] == '\n' )
         break;
      }
      fname[i] = '\0';
      fname[ i - strlen(".wfdisc")] = '\0';
 
      if (dbopen( fname, "r+", &tmpdb) < 0)  {
         unlink( ".tmp" ); 
         die (0, "Can't open database %s\n", fname );
      }
 
      tmpdb = dblookup (tmpdb, 0, "wfdisc", 0, 0);
      if (tmpdb.table < 0)  {
        unlink( ".tmp" ); 
        die (0, "Can't open wfdisc table '%s'\n", fname );
      }

     /* subset db to only look at waveform segments that matter */
      sprintf(subset_condition,"(endtime >= %lf && time <= %lf)",
                        stime, etime);
      dbsub = dbsubset(tmpdb,subset_condition,0);

 
      dbquery (dbsub, dbRECORD_COUNT, &nrec);

      if( nrec <= 0 )  continue;
      
/*             
         if (dbopen( fname, "r+", &dbsub) < 0)  {
            unlink( ".tmp" ); 
            die (0, "Can't open database %s\n", fname );
         }
 
         dbsub = dblookup (dbsub, 0, "wfdisc", 0, 0);
         if (dbsub.table < 0)  {
           unlink( ".tmp" ); 
           die (0, "Can't open wfdisc table '%s'\n", fname );
         }
 
        dbquery (dbsub, dbRECORD_COUNT, &nrec);
         if( nrec <= 0 ) 
            die( 0, " no record after dbsudset\n");
      }
*/
      for( dbsub.record = 0; dbsub.record < nrec; dbsub.record++ )  {
  
          if ( dbgetv ( dbsub, 0, "wfdisc", &wfddb, 0 ) == dbINVALID )   {
               complain( 1,"Can't get wfddb\n" );
               break;
          } 
          if ( ( wfdrec = dbget ( wfddb, rec )) == dbINVALID )   {
             complain( 1,"Can't get dbsub record #%d\n", wfddb.record );
             break;
          }
          
/*
         if ( ( wfdrec = dbget ( dbsub, rec )) == dbINVALID )   {
             complain( 1,"Can't get tmpdb record #%d\n", dbsub.record );
             break;
          }

*/ 
          db.record = dbaddnull( db ); 
          dbput ( db, rec );
          
/*
          if( dbadd ( db, rec ) == dbINVALID) {
                complain(0,"Cannot add row to working database\n");
                break;
          } 
*/ 

          dbgetv( db, 0,
	          "dir", odir, 0 );
		  
	  pathfrname( fname, newdir );
          if( odir[0] != '/' )  {
	     if( strlen( newdir ) > 0 )  {
	        strcat( newdir, "/" );
	        strcat( newdir, odir );
                if( Find_path( newdir, &str ) < 0 ) {
                    complain(0, "can't find a data path for record #%d\n",
                                  dbsub.record);
                    continue;
                }
                dbputv( db, 0, "dir", str, 0 );
	     }
	  }

       }
       /*dbfree( dbsub );*/ 

   }
   unlink( ".tmp" ); 
  
   dbquery (db, dbRECORD_COUNT, &nrec);
   if( nrec <= 0 )
      die( 0, "No records in dbin.\n");

   sort_sta_ch_tm = strtbl("sta", "chan", "time", 0 ) ;
   db = dbsort (db, sort_sta_ch_tm, 0, 0 ) ; 

   if(!mkfname( &out_dbase, stime, out_dir))  {
        die( 0, " can't make outdb\n");
   } 
   
   if (dbopen_database ( out_dbase, "r+", &dbout) < 0)
        die (0, "Can't open output database %s\n", out_dbase);
   if (dbout.table < 0) {
        dbout = dblookup (dbout, 0, "wfdisc", 0, 0);
        if (dbout.table < 0)
            die (0, "Can't open wfdisc table '%s'\n", out_dbase);
   }
 

   Seq = 1;
   if(!get_data( stime, etime ))  {
          complain(0, "error in get_data()\n");
          system(CLEAN); 
          exit(1);
   }

/*
   if( unlink( in_dbase ) < 0 )
     die( 1, "can't remove %s\n", in_dbase);
*/

  exit(0);
}

get_wfd_name(dbname, wfd_name)
char *dbname;
char wfd_name[512];
{

   Dbptr db;
   Dbvalue dbv;
   struct stat buf;
   char name[512];

   if (dbopen (dbname, "r", &db) == dbINVALID) {
      complain (0, "Unable to open database %s.\n",
                                                 dbname);
      complain (0, "Trying wfdisc name %s.wfdisc\n", dbname);
      sprintf(name, "%s.wfdisc\0", dbname);
      if(stat(name, &buf) == 0)  {
         strcpy(wfd_name, name);
      }  else
         die ( 1, "The %s doesn't exist.\n", name); 
         
    }  else {
       db = dblookup (db, 0, "wfdisc", 0, 0);
       dbquery (db, dbTABLE_FILENAME , &dbv);
       strcpy(wfd_name, dbv.t);
    } 
}
