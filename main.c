#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>

/* Read all available inotify events from the file descriptor 'fd'.
  wd is the table of watch descriptors for the directories in argv.
  argc is the length of wd and argv.
  argv is the list of watched directories.
  Entry 0 of wd and argv is unused. */

static void
handle_events(int fd, int *wd, int argc, char* argv[])
{
   /* Some systems cannot read integer variables if they are not
      properly aligned. On other systems, incorrect alignment may
      decrease performance. Hence, the buffer used for reading from
      the inotify file descriptor should have the same alignment as
      struct inotify_event. */

   char buf[4096]
       __attribute__ ((aligned(__alignof__(struct inotify_event))));
   const struct inotify_event *event;
   ssize_t len;
   char eventMessage[4096];

   /* Loop while events can be read from inotify file descriptor. */

   for (;;) {

       /* Read some events. */

       len = read(fd, buf, sizeof(buf));
       if (len == -1 && errno != EAGAIN) {
           perror("read");
           exit(EXIT_FAILURE);
       }

       /* If the nonblocking read() found no events to read, then
          it returns -1 with errno set to EAGAIN. In that case,
          we exit the loop. */

       if (len <= 0)
           break;

       /* Loop over all events in the buffer. */

       for (char *ptr = buf; ptr < buf + len;
               ptr += sizeof(struct inotify_event) + event->len) {

           event = (const struct inotify_event *) ptr;

           /* Print event type. */

           if (event->mask & IN_OPEN)
               strcat(eventMessage, "IN_OPEN: ");
           if (event->mask & IN_CLOSE_NOWRITE)
               strcat(eventMessage, "IN_CLOSE_WRITE: ");
           if (event->mask & IN_CLOSE_WRITE)
               strcat(eventMessage, "IN_CLOSE_WRITE: ");

           /* Print the name of the watched directory. */

           for (int i = 1; i < argc; ++i) {
               if (wd[i] == event->wd) {
                   strcat(eventMessage, argv[i]);
                   strcat(eventMessage, "/");
                   break;
               }
           }

           /* Print the name of the file. */

           if (event->len)
               strcat(eventMessage, event->name);

           /* Print type of filesystem object. */

           if (event->mask & IN_ISDIR)
               strcat(eventMessage, " [directory]\n");
           else
               strcat(eventMessage, " [file]\n");
           syslog(LOG_INFO, "%s", eventMessage);
       }
   }
}

int
main(int argc, char* argv[])
{
   daemon(0,0);
   char buf;
   int fd, i, poll_num;
   int *wd;
   nfds_t nfds;
   struct pollfd fds[2];

   if (argc < 2) {
       printf("Usage: %s PATH [PATH ...]\n", argv[0]);
       syslog(LOG_ERR, "Usage: PATH [PATH ...]\n");
       exit(EXIT_FAILURE);
   }


   /* Create the file descriptor for accessing the inotify API. */

   fd = inotify_init1(IN_NONBLOCK);
   if (fd == -1) {
       perror("inotify_init1");
       exit(EXIT_FAILURE);
   }

   /* Allocate memory for watch descriptors. */

   wd = calloc(argc, sizeof(int));
   if (wd == NULL) {
       perror("calloc");
       exit(EXIT_FAILURE);
   }

   /* Mark directory for event
      - file was written to (File opened for writing was closed)
   */

     wd[1] = inotify_add_watch(fd, argv[1],
                               IN_CLOSE_WRITE);
     if (wd[1] == -1) {
         fprintf(stderr, "Cannot watch '%s': %s\n",
                 argv[1], strerror(errno));
         exit(EXIT_FAILURE);
     }

   /* Prepare for polling. */

   nfds = 2;

   fds[1].fd = fd;                 /* Inotify input */
   fds[1].events = POLLIN;

   /* Wait for events and/or terminal input. */

   printf("Listening for events.\n");
   while (1) {
       poll_num = poll(fds, nfds, -1);
       if (poll_num == -1) {
           if (errno == EINTR)
               continue;
           perror("poll");
           exit(EXIT_FAILURE);
       }

       if (poll_num > 0) {

           if (fds[1].revents & POLLIN) {

               /* Inotify events are available. */
               system("touch /home/chris/Documents/test/amazing");
               handle_events(fd, wd, argc, argv);
           }
       }
   }

   printf("Listening for events stopped.\n");

   /* Close inotify file descriptor. */

   close(fd);

   free(wd);
   exit(EXIT_SUCCESS);
}
