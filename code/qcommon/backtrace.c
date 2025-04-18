#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

// gcc backtrace.c -g -rdynamic
// ./a.out
// <prints backtrace with func names>

void Com_Printf( const char *fmt, ... );

#define SIZE 20

/* Obtain a backtrace and print it to stdout. */
void print_trace (void) {
  void *array[SIZE];
  char **strings;
  int size, i;

  size = backtrace (array, SIZE);
  strings = backtrace_symbols (array, size);
  if (strings != NULL)
  {

    Com_Printf ("Obtained %d stack frames.\n", size);
    for (i = 0; i < size; i++)
      Com_Printf ("%s\n", strings[i]);
  }

  free (strings);
}

/* A dummy function to make the backtrace more interesting. */
/*
void
dummy_function (void)
{
  print_trace ();
}

int
main (void)
{
  dummy_function ();
  return 0;
}*/
