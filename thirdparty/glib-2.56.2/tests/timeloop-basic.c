#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <poll.h>

#define TRUE 1
#define FALSE 0

static int n_children = 3;
static int n_active_children;
static int n_iters = 10000;

static int write_fds[1024];
static struct pollfd poll_fds[1024];

void
my_pipe (int *fds)
{
  if (pipe(fds) < 0)
    {
      int errsv = errno;
      fprintf (stderr, "Cannot create pipe %s\n", strerror (errsv));
      exit (1);
    }
}

int
read_all (int fd, char *buf, int len)
{
  size_t bytes_read = 0;
  gssize count;

  while (bytes_read < len)
    {
      count = read (fd, buf + bytes_read, len - bytes_read);
      if (count < 0)
	{
	  if (errno != EAGAIN)
	    return FALSE;
	}
      else if (count == 0)
	return FALSE;

      bytes_read += count;
    }

  return TRUE;
}

int
write_all (int fd, char *buf, int len)
{
  size_t bytes_written = 0;
  gssize count;

  while (bytes_written < len)
    {
      count = write (fd, buf + bytes_written, len - bytes_written);
      if (count < 0)
	{
	  if (errno != EAGAIN)
	    return FALSE;
	}

      bytes_written += count;
    }

  return TRUE;
}

void
run_child (int in_fd, int out_fd)
{
  int i;
  int val = 1;

  for (i = 0; i < n_iters; i++)
    {
      write_all (out_fd, (char *)&val, sizeof (val));
      read_all (in_fd, (char *)&val, sizeof (val));
    }

  val = 0;
  write_all (out_fd, (char *)&val, sizeof (val));

  exit (0);
}

int
input_callback (int source, int dest)
{
  int val;
  
  if (!read_all (source, (char *)&val, sizeof(val)))
    {
      fprintf (stderr,"Unexpected EOF\n");
      exit (1);
    }

  if (val)
    {
      write_all (dest, (char *)&val, sizeof(val));
      return TRUE;
    }
  else
    {
      close (source);
      close (dest);
      
      n_active_children--;
      return FALSE;
    }
}

void
create_child (int pos)
{
  int pid, errsv;
  int in_fds[2];
  int out_fds[2];
  
  my_pipe (in_fds);
  my_pipe (out_fds);

  pid = fork ();
  errsv = errno;

  if (pid > 0)			/* Parent */
    {
      close (in_fds[0]);
      close (out_fds[1]);

      write_fds[pos] = in_fds[1];
      poll_fds[pos].fd = out_fds[0];
      poll_fds[pos].events = POLLIN;
    }
  else if (pid == 0)		/* Child */
    {
      close (in_fds[1]);
      close (out_fds[0]);

      setsid ();

      run_child (in_fds[0], out_fds[1]);
    }
  else				/* Error */
    {
      fprintf (stderr,"Cannot fork: %s\n", strerror (errsv));
      exit (1);
    }
}

static double 
difftimeval (struct timeval *old, struct timeval *new)
{
  return
    (new->tv_sec - old->tv_sec) * 1000. + (new->tv_usec - old->tv_usec) / 1000;
}

int 
main (int argc, char **argv)
{
  int i, j;
  struct rusage old_usage;
  struct rusage new_usage;
  
  if (argc > 1)
    n_children = atoi(argv[1]);

  if (argc > 2)
    n_iters = atoi(argv[2]);

  printf ("Children: %d     Iters: %d\n", n_children, n_iters);

  n_active_children = n_children;
  for (i = 0; i < n_children; i++)
    create_child (i);

  getrusage (RUSAGE_SELF, &old_usage);

  while (n_active_children > 0)
    {
      int old_n_active_children = n_active_children;

      poll (poll_fds, n_active_children, -1);

      for (i=0; i<n_active_children; i++)
	{
	  if (poll_fds[i].events & (POLLIN | POLLHUP))
	    {
	      if (!input_callback (poll_fds[i].fd, write_fds[i]))
		write_fds[i] = -1;
	    }
	}

      if (old_n_active_children > n_active_children)
	{
	  j = 0;
	  for (i=0; i<old_n_active_children; i++)
	    {
	      if (write_fds[i] != -1)
		{
		  if (j < i)
		    {
		      poll_fds[j] = poll_fds[i];
		      write_fds[j] = write_fds[i];
		    }
		  j++;
		}
	    }
	}
    }

  getrusage (RUSAGE_SELF, &new_usage);

  printf ("Elapsed user: %g\n",
	   difftimeval (&old_usage.ru_utime, &new_usage.ru_utime));
  printf ("Elapsed system: %g\n",
	   difftimeval (&old_usage.ru_stime, &new_usage.ru_stime));
  printf ("Elapsed total: %g\n",
	  difftimeval (&old_usage.ru_utime, &new_usage.ru_utime) +	   
	   difftimeval (&old_usage.ru_stime, &new_usage.ru_stime));
  printf ("total / iteration: %g\n",
	   (difftimeval (&old_usage.ru_utime, &new_usage.ru_utime) +	   
	    difftimeval (&old_usage.ru_stime, &new_usage.ru_stime)) /
	   (n_iters * n_children));

  return 0;
}

