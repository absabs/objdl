/*  Init
 *
 *  This routine is the initialization task for this test program.
 *  It is called from init_exec and has the responsibility for creating
 *  and starting the tasks that make up the test.  If the time of day
 *  clock is required for the test, it should also be set to a known
 *  value by this function.
 *
 *  Input parameters:  NONE
 *
 *  Output parameters:  NONE
 *
 *  COPYRIGHT (c) 1989-2009.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id: init.c,v 1.23 2009/08/12 20:50:37 joel Exp $
 */

#define CONFIGURE_INIT
#include "system.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <rtems.h>
#include <fcntl.h>
#include <rtems/error.h>
#include <ctype.h>
#include <rtems/libcsupport.h>
#include <rtems/shell.h>

void writeFile(
  const char *name,
  int         mode,
  const char *contents
)
{
  int sc;
  sc = setuid(0);
  if ( sc ) {
    printf( "setuid failed: %s:\n", name, strerror(errno) );
  }

  rtems_shell_write_file( name, contents );

  sc = chmod ( name, mode );
  if ( sc ) {
    printf( "chmod %s: %s:\n", name, strerror(errno) );
  }
}

/*
** elf file load command
*/
int load_command(int argc, char *argv[])
{
   int                status; 
   unsigned long int  start_addr;   
   void               (*fp)(void);

   /*
   ** Loading elf file
   */
   if (argc != 3)
   {
       printf("Command Usage: load lib symbol\n");
       return(-1);
   }
   else
   {
     demo(argv[1], argv[2]); 
   }	

   return(0);

}
/*
** function to start the shell and add new commands.
*/
void rtems_add_local_cmds(void)
{
   
   /*
   ** Add commands
   */ 

   rtems_shell_add_cmd("load","misc","Load statuc module",load_command);
  
}
#define writeScript( _name, _contents ) \
        writeFile( _name, 0777, _contents )

extern int _binary_tarfile_start;
extern int _binary_tarfile_size;
#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size
void fileio_start_shell(void)
{
  int sc;

  sc = mkdir("/scripts", 0777);
  if ( sc ) {
    printf( "mkdir /scripts: %s:\n", strerror(errno) );
  }

  sc = mkdir("/etc", 0777);
  if ( sc ) {
    printf( "mkdir /etc: %s:\n", strerror(errno) );
  }

  printf(
    "Creating /etc/passwd and group with three useable accounts\n"
    "root/pwd , test/pwd, rtems/NO PASSWORD\n"
  );

  writeFile(
    "/etc/passwd",
    0644,
    "root:7QR4o148UPtb.:0:0:root::/:/bin/sh\n"
    "rtems:*:1:1:RTEMS Application::/:/bin/sh\n"
    "test:8Yy.AaxynxbLI:2:2:test account::/:/bin/sh\n"
    "tty:!:3:3:tty owner::/:/bin/false\n"
  );
  writeFile(
    "/etc/group",
    0644,
    "root:x:0:root\n"
    "rtems:x:1:rtems\n"
    "test:x:2:test\n"
    "tty:x:3:tty\n"
  );

  writeScript(
    "/scripts/js", 
    "#! joel\n"
    "\n"
    "date\n"
    "echo Script successfully ran\n"
    "date\n"
    "stackuse\n"
  );

  writeScript(
    "/scripts/j1", 
    "#! joel -s 20480 -t JESS\n"
    "stackuse\n"
  );

  rtems_shell_write_file(
    "/scripts/j2", 
    "echo j2 TEST FILE\n"
    "echo j2   SHOULD BE non-executable AND\n"
    "echo j2   DOES NOT have the magic first line\n"
  );

  printf("Populating Root file system from TAR file.\n");
  Untar_FromMemory(
        (unsigned char *)(&TARFILE_START), 
        (unsigned long)&TARFILE_SIZE);
  printf("Adding Local Commands.\n");   
  rtems_add_local_cmds();
  printf(" =========================\n");
  printf(" starting shell\n");
  printf(" =========================\n");
  rtems_shell_init(
    "SHLL",                          /* task_name */
    RTEMS_MINIMUM_STACK_SIZE * 4,    /* task_stacksize */
    100,                             /* task_priority */
    "/dev/console",                  /* devname */
    false,                           /* forever */
    true,                            /* wait */
    rtems_shell_login_check          /* login */
  );
}


/*
 * RTEMS Startup Task
 */
rtems_task
Init (rtems_task_argument ignored)
{
  puts( "\n\n*** FILE I/O SAMPLE AND TEST ***" );

  fileio_start_shell ();
}

/*
 *  RTEMS Shell Configuration -- Add a command and an alias for it
 */

int main_usercmd(int argc, char **argv)
{
  int i;
  printf( "UserCommand: argc=%d\n", argc );
  for (i=0 ; i<argc ; i++ )
    printf( "argv[%d]= %s\n", i, argv[i] );
  return 0;
}

rtems_shell_cmd_t Shell_USERCMD_Command = {
  "usercmd",                                       /* name */
  "usercmd n1 [n2 [n3...]]     # echo arguments",  /* usage */
  "user",                                          /* topic */
  main_usercmd,                                    /* command */
  NULL,                                            /* alias */
  NULL                                             /* next */
};

rtems_shell_alias_t Shell_USERECHO_Alias = {
  "usercmd",                 /* command */
  "userecho"                 /* alias */
};
  

#define CONFIGURE_SHELL_USER_COMMANDS &Shell_USERCMD_Command
#define CONFIGURE_SHELL_USER_ALIASES &Shell_USERECHO_Alias
#define CONFIGURE_SHELL_COMMANDS_INIT
#define CONFIGURE_SHELL_COMMANDS_ALL

#include <rtems/shellconfig.h>
