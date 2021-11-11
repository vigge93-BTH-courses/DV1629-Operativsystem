#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>

#define PERMS 0644
struct my_msgbuf {
   long mtype;
   int mtext;
};

int main(void) {
   struct my_msgbuf buf;
   int msqid;
   int len;
   key_t key;
   system("touch msgq.txt");

   if ((key = ftok("msgq.txt", 'B')) == -1) {
      perror("ftok");
      exit(1);
   }

   if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }
   printf("message queue: ready to send messages.\n");
   buf.mtype = 1; /* we don't really care in this case */

   for(int i = 0; i < 50; i++) {
      buf.mtext = rand() - (INT_MAX / 2);
      if (msgsnd(msqid, &buf, sizeof buf.mtext, 0) == -1) /* 4 chars in an int */
         perror("msgsnd");
   }
   printf("message queue: done sending messages.\n");
   getchar();
   if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
   }
   return 0;
}
