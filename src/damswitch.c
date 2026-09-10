/*     One place that knows what this binary can be told to do.
 *
 *     The architecture audit counted 139 CCX_* switches, 94 of them with
 *     nothing written about them anywhere but the line that reads them, and
 *     no run stating which of them were actually in force.  That combination
 *     produces a specific and expensive failure: an A/B that is not wrong but
 *     UNINFORMATIVE, because the flag under test was never read.  It happened
 *     in this project more than once - a misspelt name, a variable unset by a
 *     wrapper, a Fortran banner still sitting in a buffer - and each time it
 *     cost a run to find out.
 *
 *     damswitch_report() prints, once, every switch that is set and every
 *     CCX_* name in the environment that this binary does NOT read.  The list
 *     of names is generated from the sources by tools/mkswitches.py, so it
 *     cannot drift: --check fails the regression gate if it is stale.
 *
 *     It reads nothing, decides nothing and changes no trajectory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CalculiX.h"
#include "damswitch_list.h"

extern char **environ;

/*     Names that start with CCX_ but are not switches, so that the unknown
 *     report stays worth reading.  CCX_EXE is set by the run scripts to
 *     choose the binary; CCX_JOBNAME_GETJOBNAME is set by ccx_2.23.c itself
 *     with putenv so that getjobname can find the job name.  Neither is ever
 *     read as a switch, and warning about them every run would train the
 *     reader to ignore the warning, which is the only way this report can
 *     fail. */
static const char *const damswitch_notaswitch[]={
  "CCX_EXE",
  "CCX_JOBNAME_GETJOBNAME",
  NULL
};

static ITG damswitch_ignored(const char *name,size_t n){
  ITG i;
  for(i=0;damswitch_notaswitch[i]!=NULL;i++){
    if((strlen(damswitch_notaswitch[i])==n)&&
       (strncmp(damswitch_notaswitch[i],name,n)==0)) return 1;
  }
  return 0;
}

static ITG damswitch_known(const char *name,size_t n){
  ITG i;
  for(i=0;i<DAMSWITCH_COUNT;i++){
    if((strlen(damswitch_name[i])==n)&&(strncmp(damswitch_name[i],name,n)==0))
      return 1;
  }
  return 0;
}

/* Self test: the generated list must be sorted, unique and non-empty, since
   nothing else checks it at runtime and a duplicated or empty entry would
   make the unknown-name report lie.  Returns the number of failures. */
ITG damswitch_selftest(void){
  ITG i,bad=0;
  if(DAMSWITCH_COUNT<=0){
    printf("[SWITCHES] *ERROR: the generated list is empty\n");return 1;}
  for(i=0;i<DAMSWITCH_COUNT;i++){
    if((damswitch_name[i]==NULL)||(damswitch_name[i][0]=='\0')){
      printf("[SWITCHES] *ERROR: entry %" ITGFORMAT " is empty\n",i);bad++;continue;}
    if(strncmp(damswitch_name[i],"CCX_",4)!=0){
      printf("[SWITCHES] *ERROR: %s does not start with CCX_\n",
             damswitch_name[i]);bad++;}
    if((i>0)&&(strcmp(damswitch_name[i-1],damswitch_name[i])>=0)){
      printf("[SWITCHES] *ERROR: %s and %s are out of order or duplicated\n",
             damswitch_name[i-1],damswitch_name[i]);bad++;}
  }
  /* the report must recognise a name that is in the list and reject one that
     is not, which is the only behaviour anything downstream depends on */
  if(!damswitch_known(damswitch_name[0],strlen(damswitch_name[0]))){
    printf("[SWITCHES] *ERROR: a listed name is not recognised\n");bad++;}
  if(damswitch_known("CCX_THIS_IS_NOT_A_SWITCH",24)){
    printf("[SWITCHES] *ERROR: an unlisted name is recognised\n");bad++;}
  if(!damswitch_ignored("CCX_EXE",7)){
    printf("[SWITCHES] *ERROR: the ignore list does not cover CCX_EXE\n");bad++;}
  if(damswitch_ignored("CCX_THIS_IS_NOT_A_SWITCH",24)){
    printf("[SWITCHES] *ERROR: the ignore list swallows an unknown name\n");
    bad++;}
  for(i=0;damswitch_notaswitch[i]!=NULL;i++){
    if(damswitch_known(damswitch_notaswitch[i],
                       strlen(damswitch_notaswitch[i]))){
      printf("[SWITCHES] *ERROR: %s is both read and ignored\n",
             damswitch_notaswitch[i]);bad++;}
  }
  return bad;
}

void damswitch_report(void){
  char **e;
  ITG narmed=0,nunknown=0;
  if(damswitch_selftest()!=0){
    printf("[SWITCHES] the registry self test failed; reporting nothing "
           "rather than reporting something that may be wrong\n");
    fflush(stdout);
    return;
  }
  printf("[SWITCHES] this binary reads %d CCX_* names.  In force now:\n",
         DAMSWITCH_COUNT);
  for(e=environ;*e!=NULL;e++){
    char *eq=strchr(*e,'=');
    size_t n;
    if(eq==NULL) continue;
    n=(size_t)(eq-*e);
    if((n<4)||(strncmp(*e,"CCX_",4)!=0)) continue;
    if(damswitch_known(*e,n)){
      printf("[SWITCHES]   %.*s = %s\n",(int)n,*e,eq+1);
      narmed++;
    }else if(!damswitch_ignored(*e,n)){
      nunknown++;
    }
  }
  if(narmed==0) printf("[SWITCHES]   (none - stock behaviour)\n");
  if(nunknown>0){
    printf("[SWITCHES] *WARNING: %" ITGFORMAT " CCX_* name(s) are set that "
           "this binary does NOT read.  A run configured with one of these "
           "is not testing what it looks like it is testing:\n",nunknown);
    for(e=environ;*e!=NULL;e++){
      char *eq=strchr(*e,'=');
      size_t n;
      if(eq==NULL) continue;
      n=(size_t)(eq-*e);
      if((n<4)||(strncmp(*e,"CCX_",4)!=0)) continue;
      if((!damswitch_known(*e,n))&&(!damswitch_ignored(*e,n)))
        printf("[SWITCHES]   UNKNOWN %.*s\n",(int)n,*e);
    }
  }
  fflush(stdout);
}
